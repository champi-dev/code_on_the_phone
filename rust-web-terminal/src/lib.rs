mod terminal;
mod websocket;
mod auth;
mod utils;

use wasm_bindgen::prelude::*;
use web_sys::console;

#[wasm_bindgen(start)]
pub fn main() {
    console_error_panic_hook::set_once();
    console::log_1(&"Rust Web Terminal initialized".into());
}

#[wasm_bindgen]
pub struct WebTerminal {
    terminal: terminal::Terminal,
    websocket: Option<websocket::WebSocketClient>,
}

#[wasm_bindgen]
impl WebTerminal {
    #[wasm_bindgen(constructor)]
    pub fn new(container_id: &str) -> Result<WebTerminal, JsValue> {
        let terminal = terminal::Terminal::new(container_id)?;
        
        Ok(WebTerminal {
            terminal,
            websocket: None,
        })
    }
    
    pub async fn connect(&mut self, host: &str, password: &str) -> Result<(), JsValue> {
        let ws_client = websocket::WebSocketClient::new(host)?;
        
        // Connect and authenticate
        ws_client.connect().await?;
        auth::authenticate(&ws_client, password).await?;
        
        // Set up terminal integration
        self.terminal.attach_websocket(&ws_client)?;
        
        self.websocket = Some(ws_client);
        Ok(())
    }
    
    pub fn disconnect(&mut self) {
        if let Some(ws) = &self.websocket {
            ws.close();
        }
        self.websocket = None;
    }
    
    pub fn resize(&self, cols: u16, rows: u16) -> Result<(), JsValue> {
        self.terminal.resize(cols, rows)?;
        
        if let Some(ws) = &self.websocket {
            ws.send_resize(cols, rows)?;
        }
        
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use wasm_bindgen_test::*;
    
    wasm_bindgen_test_configure!(run_in_browser);
    
    #[wasm_bindgen_test]
    fn test_terminal_creation() {
        let terminal = WebTerminal::new("test-container");
        assert!(terminal.is_ok());
    }
}
