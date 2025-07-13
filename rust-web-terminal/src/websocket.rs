use wasm_bindgen::prelude::*;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::JsFuture;
use web_sys::{WebSocket, MessageEvent, ErrorEvent, CloseEvent, BinaryType};
use std::cell::RefCell;
use std::rc::Rc;

type OutputHandler = Box<dyn FnMut(&str)>;

#[derive(Clone)]
pub struct WebSocketClient {
    ws: Rc<RefCell<Option<WebSocket>>>,
    url: String,
    output_handler: Rc<RefCell<Option<OutputHandler>>>,
}

impl WebSocketClient {
    pub fn new(host: &str) -> Result<WebSocketClient, JsValue> {
        let protocol = if web_sys::window()
            .and_then(|w| w.location().protocol().ok())
            .map(|p| p == "https:")
            .unwrap_or(false) 
        {
            "wss"
        } else {
            "ws"
        };
        
        let url = format!("{}://{}:7681/ws", protocol, host);
        
        Ok(WebSocketClient {
            ws: Rc::new(RefCell::new(None)),
            url,
            output_handler: Rc::new(RefCell::new(None)),
        })
    }
    
    pub async fn connect(&self) -> Result<(), JsValue> {
        let ws = WebSocket::new(&self.url)?;
        ws.set_binary_type(BinaryType::Arraybuffer);
        
        // Set up event handlers
        let ws_clone = ws.clone();
        let output_handler = self.output_handler.clone();
        
        // On message handler
        let on_message = Closure::wrap(Box::new(move |event: MessageEvent| {
            if let Ok(array_buffer) = event.data().dyn_into::<js_sys::ArrayBuffer>() {
                let array = js_sys::Uint8Array::new(&array_buffer);
                let mut data = vec![0; array.length() as usize];
                array.copy_to(&mut data);
                
                // For ttyd, first byte is message type
                if data.len() > 0 {
                    match data[0] {
                        b'0' => {
                            // Terminal output
                            if let Ok(text) = String::from_utf8(data[1..].to_vec()) {
                                if let Some(handler) = output_handler.borrow_mut().as_mut() {
                                    handler(&text);
                                }
                            }
                        }
                        b'1' => {
                            // Resize acknowledgment
                            web_sys::console::log_1(&"Terminal resized".into());
                        }
                        _ => {
                            web_sys::console::log_1(&format!("Unknown message type: {}", data[0]).into());
                        }
                    }
                }
            } else if let Ok(text) = event.data().dyn_into::<js_sys::JsString>() {
                let text: String = text.into();
                if let Some(handler) = output_handler.borrow_mut().as_mut() {
                    handler(&text);
                }
            }
        }) as Box<dyn FnMut(MessageEvent)>);
        
        ws.set_onmessage(Some(on_message.as_ref().unchecked_ref()));
        on_message.forget();
        
        // On open handler
        let (tx, rx) = futures::channel::oneshot::channel();
        let mut tx = Some(tx);
        
        let on_open = Closure::wrap(Box::new(move || {
            web_sys::console::log_1(&"WebSocket connected".into());
            if let Some(tx) = tx.take() {
                let _ = tx.send(Ok(()));
            }
        }) as Box<dyn FnMut()>);
        
        ws.set_onopen(Some(on_open.as_ref().unchecked_ref()));
        on_open.forget();
        
        // On error handler
        let on_error = Closure::wrap(Box::new(move |event: ErrorEvent| {
            web_sys::console::error_1(&format!("WebSocket error: {:?}", event).into());
        }) as Box<dyn FnMut(ErrorEvent)>);
        
        ws.set_onerror(Some(on_error.as_ref().unchecked_ref()));
        on_error.forget();
        
        // On close handler
        let on_close = Closure::wrap(Box::new(move |event: CloseEvent| {
            web_sys::console::log_1(&format!("WebSocket closed: {} - {}", event.code(), event.reason()).into());
        }) as Box<dyn FnMut(CloseEvent)>);
        
        ws.set_onclose(Some(on_close.as_ref().unchecked_ref()));
        on_close.forget();
        
        *self.ws.borrow_mut() = Some(ws);
        
        // Wait for connection
        rx.await.map_err(|_| JsValue::from_str("Failed to connect"))?
    }
    
    pub fn send_data(&self, data: &str) -> Result<(), JsValue> {
        if let Some(ws) = self.ws.borrow().as_ref() {
            if ws.ready_state() == WebSocket::OPEN {
                // For ttyd, wrap input in binary frame with type 0
                let mut bytes = vec![0u8]; // Type 0 for input
                bytes.extend_from_slice(data.as_bytes());
                
                let array = js_sys::Uint8Array::from(&bytes[..]);
                ws.send_with_array_buffer(&array.buffer())?;
            }
        }
        Ok(())
    }
    
    pub fn send_resize(&self, cols: u16, rows: u16) -> Result<(), JsValue> {
        if let Some(ws) = self.ws.borrow().as_ref() {
            if ws.ready_state() == WebSocket::OPEN {
                // ttyd resize format: type 1 + cols (2 bytes) + rows (2 bytes)
                let mut bytes = vec![1u8]; // Type 1 for resize
                bytes.extend_from_slice(&cols.to_be_bytes());
                bytes.extend_from_slice(&rows.to_be_bytes());
                
                let array = js_sys::Uint8Array::from(&bytes[..]);
                ws.send_with_array_buffer(&array.buffer())?;
            }
        }
        Ok(())
    }
    
    pub fn close(&self) {
        if let Some(ws) = self.ws.borrow().as_ref() {
            let _ = ws.close();
        }
    }
    
    pub fn set_output_handler(&self, handler: OutputHandler) -> Result<(), JsValue> {
        *self.output_handler.borrow_mut() = Some(handler);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use wasm_bindgen_test::*;
    
    #[wasm_bindgen_test]
    fn test_websocket_url_construction() {
        let client = WebSocketClient::new("142.93.249.123").unwrap();
        assert!(client.url.starts_with("ws://") || client.url.starts_with("wss://"));
        assert!(client.url.contains("142.93.249.123"));
        assert!(client.url.ends_with(":7681/ws"));
    }
}