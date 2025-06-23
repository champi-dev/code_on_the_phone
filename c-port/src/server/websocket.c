#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

/* WebSocket GUID for handshake */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* WebSocket frame header structure */
typedef struct {
    uint8_t fin : 1;
    uint8_t rsv : 3;
    uint8_t opcode : 4;
    uint8_t mask : 1;
    uint8_t payload_len : 7;
} ws_frame_header_t;

/* Base64 encoding table */
static const char b64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Fast base64 encode - optimized for SHA1 output (20 bytes) */
static void base64_encode(const unsigned char *in, size_t in_len, char *out) {
    size_t i, j;
    uint32_t buf;
    
    for (i = 0, j = 0; i < in_len - 2; i += 3) {
        buf = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
        out[j++] = b64_table[(buf >> 18) & 0x3f];
        out[j++] = b64_table[(buf >> 12) & 0x3f];
        out[j++] = b64_table[(buf >> 6) & 0x3f];
        out[j++] = b64_table[buf & 0x3f];
    }
    
    /* Handle padding */
    if (i < in_len) {
        buf = in[i] << 16;
        if (i + 1 < in_len) buf |= in[i + 1] << 8;
        
        out[j++] = b64_table[(buf >> 18) & 0x3f];
        out[j++] = b64_table[(buf >> 12) & 0x3f];
        out[j++] = (i + 1 < in_len) ? b64_table[(buf >> 6) & 0x3f] : '=';
        out[j++] = '=';
    }
    
    out[j] = '\0';
}

/* WebSocket handshake - generate accept key */
int ct_ws_handshake(ct_connection_t *conn) {
    /* Find Sec-WebSocket-Key header */
    const char *ws_key = ct_request_get_header(&conn->request, 
                                               "Sec-WebSocket-Key");
    if (!ws_key) return -1;
    
    /* Concatenate with GUID */
    char key_guid[256];
    snprintf(key_guid, sizeof(key_guid), "%s%s", ws_key, WS_GUID);
    
    /* SHA1 hash */
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)key_guid, strlen(key_guid), hash);
    
    /* Base64 encode */
    char accept_key[64];
    base64_encode(hash, SHA_DIGEST_LENGTH, accept_key);
    
    /* Build response */
    ct_response_init(&conn->response, 101, "Switching Protocols");
    ct_response_add_header(&conn->response, "Upgrade", "websocket");
    ct_response_add_header(&conn->response, "Connection", "Upgrade");
    ct_response_add_header(&conn->response, "Sec-WebSocket-Accept", accept_key);
    
    /* Check for protocol */
    const char *ws_protocol = ct_request_get_header(&conn->request,
                                                   "Sec-WebSocket-Protocol");
    if (ws_protocol) {
        ct_response_add_header(&conn->response, "Sec-WebSocket-Protocol", 
                              ws_protocol);
    }
    
    conn->is_websocket = true;
    conn->ws_handshake_done = true;
    
    return 0;
}

/* Parse WebSocket frame - optimized for common cases */
int ct_ws_parse_frame(const char *data, size_t len, ct_ws_opcode_t *opcode,
                      const char **payload, size_t *payload_len) {
    if (len < 2) return -1; /* Need at least 2 bytes for header */
    
    const uint8_t *p = (const uint8_t *)data;
    
    /* Parse first byte */
    uint8_t fin = (p[0] >> 7) & 0x01;
    uint8_t rsv = (p[0] >> 4) & 0x07;
    *opcode = (ct_ws_opcode_t)(p[0] & 0x0F);
    
    /* We don't support fragmented frames for simplicity */
    if (!fin) return -2;
    if (rsv != 0) return -2; /* No extensions supported */
    
    /* Parse second byte */
    uint8_t mask = (p[1] >> 7) & 0x01;
    uint64_t plen = p[1] & 0x7F;
    
    size_t header_len = 2;
    
    /* Extended payload length */
    if (plen == 126) {
        if (len < 4) return -1;
        plen = ntohs(*(uint16_t *)(p + 2));
        header_len = 4;
    } else if (plen == 127) {
        if (len < 10) return -1;
        /* We don't support >4GB frames */
        uint32_t high = ntohl(*(uint32_t *)(p + 2));
        if (high != 0) return -2;
        plen = ntohl(*(uint32_t *)(p + 6));
        header_len = 10;
    }
    
    /* Masking key */
    const uint8_t *mask_key = NULL;
    if (mask) {
        if (len < header_len + 4) return -1;
        mask_key = p + header_len;
        header_len += 4;
    }
    
    /* Check if we have complete frame */
    if (len < header_len + plen) return -1;
    
    *payload = (const char *)(p + header_len);
    *payload_len = plen;
    
    /* Unmask payload in-place if needed (modifies input!) */
    if (mask && plen > 0) {
        uint8_t *payload_data = (uint8_t *)*payload;
        
        /* Optimized unmasking - process 4 bytes at a time */
        size_t i;
        for (i = 0; i + 3 < plen; i += 4) {
            *(uint32_t *)(payload_data + i) ^= *(uint32_t *)mask_key;
        }
        
        /* Handle remaining bytes */
        for (; i < plen; i++) {
            payload_data[i] ^= mask_key[i & 3];
        }
    }
    
    return header_len + plen; /* Total frame size */
}

/* Build WebSocket frame - optimized for server->client (no masking) */
int ct_ws_build_frame(ct_ws_opcode_t opcode, const char *payload,
                      size_t payload_len, char *buf, size_t buf_len) {
    if (buf_len < 2 + payload_len) return -1;
    
    uint8_t *p = (uint8_t *)buf;
    size_t header_len;
    
    /* First byte: FIN=1, RSV=0, Opcode */
    p[0] = 0x80 | (opcode & 0x0F);
    
    /* Payload length encoding */
    if (payload_len < 126) {
        p[1] = payload_len;
        header_len = 2;
    } else if (payload_len < 65536) {
        if (buf_len < 4 + payload_len) return -1;
        p[1] = 126;
        *(uint16_t *)(p + 2) = htons(payload_len);
        header_len = 4;
    } else {
        if (buf_len < 10 + payload_len) return -1;
        p[1] = 127;
        *(uint32_t *)(p + 2) = 0; /* High 32 bits */
        *(uint32_t *)(p + 6) = htonl(payload_len);
        header_len = 10;
    }
    
    /* Copy payload */
    if (payload && payload_len > 0) {
        memcpy(p + header_len, payload, payload_len);
    }
    
    return header_len + payload_len;
}

/* Send WebSocket message */
int ct_ws_send_message(ct_connection_t *conn, ct_ws_opcode_t opcode,
                       const char *data, size_t len) {
    char frame[CT_BUFFER_SIZE];
    
    int frame_len = ct_ws_build_frame(opcode, data, len, frame, sizeof(frame));
    if (frame_len < 0) return -1;
    
    /* Write to connection buffer */
    return ct_ring_buffer_write(&conn->write_buf, frame, frame_len);
}

/* Send WebSocket text message */
int ct_ws_send_text(ct_connection_t *conn, const char *text) {
    return ct_ws_send_message(conn, CT_WS_TEXT, text, strlen(text));
}

/* Send WebSocket binary message */
int ct_ws_send_binary(ct_connection_t *conn, const void *data, size_t len) {
    return ct_ws_send_message(conn, CT_WS_BINARY, data, len);
}

/* Send WebSocket ping */
int ct_ws_send_ping(ct_connection_t *conn, const char *data, size_t len) {
    return ct_ws_send_message(conn, CT_WS_PING, data, len);
}

/* Send WebSocket pong */
int ct_ws_send_pong(ct_connection_t *conn, const char *data, size_t len) {
    return ct_ws_send_message(conn, CT_WS_PONG, data, len);
}

/* Send WebSocket close */
int ct_ws_send_close(ct_connection_t *conn, uint16_t code, const char *reason) {
    char payload[125 + 2]; /* Max control frame payload */
    size_t payload_len = 0;
    
    if (code > 0) {
        *(uint16_t *)payload = htons(code);
        payload_len = 2;
        
        if (reason) {
            size_t reason_len = strlen(reason);
            if (reason_len > 123) reason_len = 123;
            memcpy(payload + 2, reason, reason_len);
            payload_len += reason_len;
        }
    }
    
    return ct_ws_send_message(conn, CT_WS_CLOSE, payload, payload_len);
}

/* Process WebSocket frame */
int ct_ws_process_frame(ct_connection_t *conn, ct_ws_opcode_t opcode,
                        const char *payload, size_t payload_len) {
    switch (opcode) {
        case CT_WS_TEXT:
        case CT_WS_BINARY:
            /* Application should handle these */
            return 0;
            
        case CT_WS_CLOSE:
            /* Echo close frame and mark for closing */
            if (payload_len >= 2) {
                uint16_t code = ntohs(*(uint16_t *)payload);
                const char *reason = (payload_len > 2) ? payload + 2 : NULL;
                ct_ws_send_close(conn, code, reason);
            } else {
                ct_ws_send_close(conn, 1000, "Normal closure");
            }
            conn->state = CT_CONN_CLOSING;
            return 0;
            
        case CT_WS_PING:
            /* Reply with pong */
            return ct_ws_send_pong(conn, payload, payload_len);
            
        case CT_WS_PONG:
            /* Ignore pongs */
            return 0;
            
        default:
            /* Unknown opcode */
            ct_ws_send_close(conn, 1002, "Protocol error");
            conn->state = CT_CONN_CLOSING;
            return -1;
    }
}