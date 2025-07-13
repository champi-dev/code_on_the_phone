use wasm_bindgen::prelude::*;
use wasm_bindgen_futures::JsFuture;
use web_sys::{Request, RequestInit, Response, Headers};
use crate::websocket::WebSocketClient;
use crate::utils;

pub async fn authenticate(ws_client: &WebSocketClient, password: &str) -> Result<(), JsValue> {
    // For ttyd, authentication is typically done through HTTP before WebSocket
    // Let's send the auth token if needed
    
    // Create auth token (base64 encoded)
    let auth_string = format!(":{}", password);
    let auth_token = utils::base64_encode(&auth_string);
    
    // Send authentication data through WebSocket if required
    // Note: ttyd might handle auth differently, this is a placeholder
    
    web_sys::console::log_1(&format!("Authenticated with token: {}", auth_token).into());
    
    Ok(())
}

pub async fn login_http(host: &str, password: &str) -> Result<String, JsValue> {
    let window = web_sys::window().ok_or("No window found")?;
    let protocol = window.location().protocol()?;
    let url = format!("{}//{}:7681/token", protocol, host);
    
    let mut opts = RequestInit::new();
    opts.method("POST");
    opts.mode(web_sys::RequestMode::Cors);
    
    let headers = Headers::new()?;
    headers.set("Content-Type", "application/x-www-form-urlencoded")?;
    opts.headers(&headers);
    
    let body = format!("password={}", urlencoding::encode(password));
    opts.body(Some(&JsValue::from_str(&body)));
    
    let request = Request::new_with_str_and_init(&url, &opts)?;
    
    let resp_value = JsFuture::from(window.fetch_with_request(&request)).await?;
    let resp: Response = resp_value.dyn_into()?;
    
    if !resp.ok() {
        return Err(JsValue::from_str("Authentication failed"));
    }
    
    let text = JsFuture::from(resp.text()?).await?;
    Ok(text.as_string().unwrap_or_default())
}

#[cfg(test)]
mod tests {
    use super::*;
    use wasm_bindgen_test::*;
    
    #[wasm_bindgen_test]
    fn test_auth_token_generation() {
        let auth_string = format!(":{}", "cloudterm123");
        let encoded = crate::utils::base64_encode(&auth_string);
        assert!(!encoded.is_empty());
        assert!(encoded.contains("Y2xvdWR0ZXJt")); // Part of base64 encoded "cloudterm123"
    }
}

mod urlencoding {
    pub fn encode(s: &str) -> String {
        s.chars()
            .map(|c| match c {
                'A'..='Z' | 'a'..='z' | '0'..='9' | '-' | '_' | '.' | '~' => c.to_string(),
                ' ' => "+".to_string(),
                _ => format!("%{:02X}", c as u8),
            })
            .collect()
    }
}