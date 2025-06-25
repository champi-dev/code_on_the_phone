package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

func TestHealthEndpoint(t *testing.T) {
	req, err := http.NewRequest("GET", "/health", nil)
	if err != nil {
		t.Fatal(err)
	}

	rr := httptest.NewRecorder()
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("OK"))
	})

	handler.ServeHTTP(rr, req)

	if status := rr.Code; status != http.StatusOK {
		t.Errorf("handler returned wrong status code: got %v want %v",
			status, http.StatusOK)
	}

	expected := "OK"
	if rr.Body.String() != expected {
		t.Errorf("handler returned unexpected body: got %v want %v",
			rr.Body.String(), expected)
	}
}

func TestWebSocketConnection(t *testing.T) {
	// Create test server
	server := httptest.NewServer(http.HandlerFunc(handleWebSocket))
	defer server.Close()

	// Convert http:// to ws://
	wsURL := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"

	// Connect to WebSocket
	ws, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("Failed to connect to WebSocket: %v", err)
	}
	defer ws.Close()

	// Wait for connected message
	var msg Message
	err = ws.ReadJSON(&msg)
	if err != nil {
		t.Fatalf("Failed to read connected message: %v", err)
	}

	if msg.Type != MessageTypeOutput && msg.Type != "connected" {
		t.Errorf("Expected connected message, got: %v", msg.Type)
	}
}

func TestWebSocketEchoCommand(t *testing.T) {
	// Create test server
	server := httptest.NewServer(http.HandlerFunc(handleWebSocket))
	defer server.Close()

	wsURL := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"
	ws, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer ws.Close()

	// Wait for initial connection
	time.Sleep(100 * time.Millisecond)

	// Send echo command
	inputMsg := Message{
		Type: MessageTypeInput,
		Data: "echo 'test output'\n",
	}
	err = ws.WriteJSON(inputMsg)
	if err != nil {
		t.Fatalf("Failed to send input: %v", err)
	}

	// Read output messages
	foundOutput := false
	timeout := time.After(2 * time.Second)

	for !foundOutput {
		select {
		case <-timeout:
			t.Fatal("Timeout waiting for echo output")
		default:
			var msg Message
			err := ws.ReadJSON(&msg)
			if err != nil {
				continue
			}
			if msg.Type == MessageTypeOutput && strings.Contains(msg.Data, "test output") {
				foundOutput = true
			}
		}
	}

	if !foundOutput {
		t.Error("Did not receive expected echo output")
	}
}

func TestWebSocketResize(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(handleWebSocket))
	defer server.Close()

	wsURL := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"
	ws, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer ws.Close()

	// Send resize message
	resizeMsg := Message{
		Type: MessageTypeResize,
		Cols: 120,
		Rows: 40,
	}
	err = ws.WriteJSON(resizeMsg)
	if err != nil {
		t.Fatalf("Failed to send resize: %v", err)
	}

	// Should not receive error
	time.Sleep(100 * time.Millisecond)
}

func TestWebSocketInvalidMessage(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(handleWebSocket))
	defer server.Close()

	wsURL := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"
	ws, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer ws.Close()

	// Send invalid JSON
	err = ws.WriteMessage(websocket.TextMessage, []byte("invalid json"))
	if err != nil {
		t.Fatalf("Failed to send invalid message: %v", err)
	}

	// Connection should still be alive
	time.Sleep(100 * time.Millisecond)
	
	// Try sending valid message
	validMsg := Message{
		Type: MessageTypeInput,
		Data: "echo 'still alive'\n",
	}
	err = ws.WriteJSON(validMsg)
	if err != nil {
		t.Error("Connection closed after invalid message")
	}
}

func TestCommandAnimationDetection(t *testing.T) {
	tests := []struct {
		command   string
		expected  AnimationType
	}{
		{"ls -la", AnimationMatrixRain},
		{"cd /home", AnimationWormholePortal},
		{"git status", AnimationDNAHelix},
		{"python script.py", AnimationNeuralNetwork},
		{"ssh user@host", AnimationQuantumTunnel},
		{"regular command", ""},
	}

	for _, tt := range tests {
		result := detectAnimation(tt.command)
		if result != tt.expected {
			t.Errorf("detectAnimation(%q) = %v, want %v", tt.command, result, tt.expected)
		}
	}
}

func TestHostKeyVerification(t *testing.T) {
	config := Config{
		Droplet: struct {
			Host     string `json:"host"`
			Port     int    `json:"port"`
			Username string `json:"username"`
			KeyPath  string `json:"keyPath"`
		}{
			Host:     "test.example.com",
			Port:     22,
			Username: "testuser",
			KeyPath:  "~/.ssh/test_key",
		},
	}

	callback := getHostKeyCallback(config)
	if callback == nil {
		t.Error("getHostKeyCallback returned nil")
	}
}

func TestConcurrentConnections(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(handleWebSocket))
	defer server.Close()

	wsURL := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"

	// Create multiple connections
	connections := make([]*websocket.Conn, 3)
	for i := 0; i < 3; i++ {
		ws, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
		if err != nil {
			t.Fatalf("Failed to create connection %d: %v", i, err)
		}
		connections[i] = ws
		defer ws.Close()
	}

	// Send different commands to each
	for i, ws := range connections {
		msg := Message{
			Type: MessageTypeInput,
			Data: "echo 'connection " + string(rune('0'+i)) + "'\n",
		}
		err := ws.WriteJSON(msg)
		if err != nil {
			t.Errorf("Failed to send to connection %d: %v", i, err)
		}
	}

	// Each should receive their own output
	for i, ws := range connections {
		expectedOutput := "connection " + string(rune('0'+i))
		found := false
		timeout := time.After(2 * time.Second)

		for !found {
			select {
			case <-timeout:
				t.Errorf("Timeout waiting for output on connection %d", i)
				break
			default:
				var msg Message
				err := ws.ReadJSON(&msg)
				if err != nil {
					continue
				}
				if msg.Type == MessageTypeOutput && strings.Contains(msg.Data, expectedOutput) {
					found = true
				}
			}
		}
	}
}

func BenchmarkWebSocketMessage(b *testing.B) {
	server := httptest.NewServer(http.HandlerFunc(handleWebSocket))
	defer server.Close()

	wsURL := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"
	ws, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		b.Fatalf("Failed to connect: %v", err)
	}
	defer ws.Close()

	// Wait for connection
	time.Sleep(100 * time.Millisecond)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		msg := Message{
			Type: MessageTypeInput,
			Data: "echo 'benchmark test'\n",
		}
		ws.WriteJSON(msg)
		
		// Read response
		var response Message
		ws.ReadJSON(&response)
	}
}