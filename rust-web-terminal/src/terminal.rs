use wasm_bindgen::prelude::*;
use wasm_bindgen::JsCast;
use web_sys::{Document, Element, HtmlElement, Window};
use crate::websocket::WebSocketClient;

#[wasm_bindgen]
extern "C" {
    #[wasm_bindgen(js_namespace = Terminal)]
    type XTerm;
    
    #[wasm_bindgen(constructor, js_namespace = Terminal)]
    fn new(options: &JsValue) -> XTerm;
    
    #[wasm_bindgen(method)]
    fn open(this: &XTerm, element: &Element);
    
    #[wasm_bindgen(method)]
    fn write(this: &XTerm, data: &str);
    
    #[wasm_bindgen(method)]
    fn writeln(this: &XTerm, data: &str);
    
    #[wasm_bindgen(method)]
    fn clear(this: &XTerm);
    
    #[wasm_bindgen(method)]
    fn focus(this: &XTerm);
    
    #[wasm_bindgen(method)]
    fn resize(this: &XTerm, cols: u16, rows: u16);
    
    #[wasm_bindgen(method, js_name = onData)]
    fn on_data(this: &XTerm, callback: &Closure<dyn FnMut(String)>);
    
    #[wasm_bindgen(method, js_name = onResize)]
    fn on_resize(this: &XTerm, callback: &Closure<dyn FnMut(JsValue)>);
    
    #[wasm_bindgen(getter)]
    fn cols(this: &XTerm) -> u16;
    
    #[wasm_bindgen(getter)]
    fn rows(this: &XTerm) -> u16;
}

#[wasm_bindgen]
extern "C" {
    #[wasm_bindgen(js_namespace = ["Terminal", "FitAddon"], js_name = FitAddon)]
    type FitAddon;
    
    #[wasm_bindgen(constructor, js_namespace = ["Terminal", "FitAddon"])]
    fn new() -> FitAddon;
    
    #[wasm_bindgen(method)]
    fn fit(this: &FitAddon);
}

pub struct Terminal {
    xterm: XTerm,
    fit_addon: FitAddon,
    container: Element,
    _on_data_callback: Option<Closure<dyn FnMut(String)>>,
    _on_resize_callback: Option<Closure<dyn FnMut(JsValue)>>,
}

impl Terminal {
    pub fn new(container_id: &str) -> Result<Terminal, JsValue> {
        let window = web_sys::window().ok_or("No window found")?;
        let document = window.document().ok_or("No document found")?;
        let container = document
            .get_element_by_id(container_id)
            .ok_or("Container element not found")?;
        
        // Create xterm options
        let options = js_sys::Object::new();
        js_sys::Reflect::set(&options, &"cursorBlink".into(), &true.into())?;
        js_sys::Reflect::set(&options, &"fontSize".into(), &14.into())?;
        js_sys::Reflect::set(&options, &"scrollback".into(), &10000.into())?;
        
        // Theme configuration
        let theme = js_sys::Object::new();
        js_sys::Reflect::set(&theme, &"background".into(), &"#0d1117".into())?;
        js_sys::Reflect::set(&theme, &"foreground".into(), &"#c9d1d9".into())?;
        js_sys::Reflect::set(&theme, &"cursor".into(), &"#58a6ff".into())?;
        js_sys::Reflect::set(&theme, &"cursorAccent".into(), &"#0d1117".into())?;
        js_sys::Reflect::set(&theme, &"selection".into(), &"rgba(88, 166, 255, 0.3)".into())?;
        
        js_sys::Reflect::set(&options, &"theme".into(), &theme)?;
        
        let xterm = XTerm::new(&options.into());
        let fit_addon = FitAddon::new();
        
        // Open terminal in container
        xterm.open(&container);
        
        // Write welcome message
        xterm.writeln("Rust Web Terminal v0.1.0");
        xterm.writeln("Connecting to droplet...");
        xterm.writeln("");
        
        Ok(Terminal {
            xterm,
            fit_addon,
            container,
            _on_data_callback: None,
            _on_resize_callback: None,
        })
    }
    
    pub fn attach_websocket(&mut self, ws_client: &WebSocketClient) -> Result<(), JsValue> {
        let ws_clone = ws_client.clone();
        
        // Set up data handler
        let on_data = Closure::wrap(Box::new(move |data: String| {
            if let Err(e) = ws_clone.send_data(&data) {
                web_sys::console::error_1(&format!("Failed to send data: {:?}", e).into());
            }
        }) as Box<dyn FnMut(String)>);
        
        self.xterm.on_data(&on_data);
        self._on_data_callback = Some(on_data);
        
        // Set up resize handler
        let ws_clone2 = ws_client.clone();
        let on_resize = Closure::wrap(Box::new(move |event: JsValue| {
            if let Ok(obj) = event.dyn_into::<js_sys::Object>() {
                if let Ok(cols) = js_sys::Reflect::get(&obj, &"cols".into()) {
                    if let Ok(rows) = js_sys::Reflect::get(&obj, &"rows".into()) {
                        if let (Some(cols), Some(rows)) = (cols.as_f64(), rows.as_f64()) {
                            let _ = ws_clone2.send_resize(cols as u16, rows as u16);
                        }
                    }
                }
            }
        }) as Box<dyn FnMut(JsValue)>);
        
        self.xterm.on_resize(&on_resize);
        self._on_resize_callback = Some(on_resize);
        
        // Connect websocket output to terminal
        ws_client.set_output_handler(Box::new({
            let xterm = self.xterm.clone();
            move |data: &str| {
                xterm.write(data);
            }
        }))?;
        
        Ok(())
    }
    
    pub fn write(&self, data: &str) {
        self.xterm.write(data);
    }
    
    pub fn writeln(&self, data: &str) {
        self.xterm.writeln(data);
    }
    
    pub fn clear(&self) {
        self.xterm.clear();
    }
    
    pub fn focus(&self) {
        self.xterm.focus();
    }
    
    pub fn resize(&self, cols: u16, rows: u16) -> Result<(), JsValue> {
        self.xterm.resize(cols, rows);
        self.fit_addon.fit();
        Ok(())
    }
    
    pub fn fit(&self) {
        self.fit_addon.fit();
    }
}

// Make XTerm cloneable for use in closures
impl Clone for XTerm {
    fn clone(&self) -> Self {
        self.clone()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use wasm_bindgen_test::*;
    
    #[wasm_bindgen_test]
    fn test_terminal_options() {
        let options = js_sys::Object::new();
        assert!(js_sys::Reflect::set(&options, &"cursorBlink".into(), &true.into()).is_ok());
        assert!(js_sys::Reflect::set(&options, &"fontSize".into(), &14.into()).is_ok());
    }
}