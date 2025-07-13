#[cfg(test)]
mod tests {
    use wasm_bindgen_test::*;
    
    wasm_bindgen_test_configure!(run_in_browser);
    
    #[wasm_bindgen_test]
    fn test_base64_encoding() {
        use rust_web_terminal::utils;
        
        let input = "cloudterm123";
        let encoded = utils::base64_encode(input);
        assert_eq!(encoded, "Y2xvdWR0ZXJtMTIz");
        
        let decoded = utils::base64_decode(&encoded).unwrap();
        assert_eq!(String::from_utf8(decoded).unwrap(), input);
    }
    
    #[wasm_bindgen_test]
    fn test_websocket_url_construction() {
        // This would need to be tested with the actual WebSocketClient
        // but we can test the URL format logic
        let host = "142.93.249.123";
        let expected_ws = format!("ws://{}:7681/ws", host);
        let expected_wss = format!("wss://{}:7681/ws", host);
        
        assert!(expected_ws.contains(host));
        assert!(expected_ws.ends_with(":7681/ws"));
        assert!(expected_wss.contains(host));
        assert!(expected_wss.ends_with(":7681/ws"));
    }
    
    #[wasm_bindgen_test]
    fn test_auth_token_format() {
        use rust_web_terminal::utils;
        
        let password = "cloudterm123";
        let auth_string = format!(":{}", password);
        let token = utils::base64_encode(&auth_string);
        
        // Token should be base64 encoded ":password"
        assert!(!token.is_empty());
        assert!(token.len() > password.len()); // Base64 is longer
    }
    
    #[wasm_bindgen_test]
    async fn test_terminal_lifecycle() {
        use rust_web_terminal::WebTerminal;
        
        // Create a test container in the DOM
        let document = web_sys::window().unwrap().document().unwrap();
        let container = document.create_element("div").unwrap();
        container.set_id("test-terminal");
        document.body().unwrap().append_child(&container).unwrap();
        
        // Create terminal
        let terminal = WebTerminal::new("test-terminal");
        assert!(terminal.is_ok());
        
        let mut terminal = terminal.unwrap();
        
        // Test disconnect (should not panic even if not connected)
        terminal.disconnect();
        
        // Clean up
        container.remove();
    }
}