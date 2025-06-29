use axum::{
    extract::ws::{Message, WebSocket, WebSocketUpgrade},
    response::IntoResponse,
    routing::get,
    Router,
};
use serde::{Deserialize, Serialize};
use tower_http::services::ServeDir;
// use uuid::Uuid;
use tokio::sync::mpsc;
use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::Mutex;
use tokio::sync::Mutex as AsyncMutex;
use quantum_terminal::session::{shell::{create_shell_session, PersistentShell}, manager::SessionManager};
// use quantum_terminal::core::metrics::Metrics;

#[derive(Debug, Serialize, Deserialize)]
#[serde(tag = "type")]
enum WsMessage {
    #[serde(rename = "session")]
    Session { id: String },
    #[serde(rename = "output")]
    Output { content: String },
    #[serde(rename = "error")]
    Error { content: String },
    #[serde(rename = "command")]
    Command { content: String },
}

pub struct WebServer {
    port: u16,
    sessions: Arc<Mutex<HashMap<String, mpsc::UnboundedSender<String>>>>,
    shells: Arc<Mutex<HashMap<String, Arc<AsyncMutex<PersistentShell>>>>>,
    session_manager: Arc<SessionManager>,
}

impl WebServer {
    pub fn new(port: u16, session_manager: Arc<SessionManager>) -> Self {
        Self { 
            port,
            sessions: Arc::new(Mutex::new(HashMap::new())),
            shells: Arc::new(Mutex::new(HashMap::new())),
            session_manager,
        }
    }

    pub async fn run(self) -> Result<(), Box<dyn std::error::Error>> {
        // Get absolute path to static directory
        let static_dir = std::env::current_dir()?.join("static");
        println!("Serving static files from: {:?}", static_dir);
        
        let sessions = self.sessions.clone();
        let shells = self.shells.clone();
        let session_manager = self.session_manager.clone();
        let app = Router::new()
            .route("/ws", get(move |ws| websocket_handler(ws, sessions.clone(), shells.clone(), session_manager.clone())))
            .route("/", get(serve_index))
            .fallback_service(ServeDir::new(static_dir));

        let addr = format!("0.0.0.0:{}", self.port);
        println!("Web server listening on http://{}", addr);
        
        let listener = tokio::net::TcpListener::bind(&addr).await?;
        axum::serve(listener, app).await?;

        Ok(())
    }
}

async fn serve_index() -> impl IntoResponse {
    match tokio::fs::read_to_string("static/index.html").await {
        Ok(html) => axum::response::Html(html),
        Err(_) => axum::response::Html("<h1>Error: Could not load index.html</h1>".to_string()),
    }
}

async fn websocket_handler(
    ws: WebSocketUpgrade,
    sessions: Arc<Mutex<HashMap<String, mpsc::UnboundedSender<String>>>>,
    shells: Arc<Mutex<HashMap<String, Arc<AsyncMutex<PersistentShell>>>>>,
    session_manager: Arc<SessionManager>,
) -> impl IntoResponse {
    ws.on_upgrade(move |socket| handle_socket(socket, sessions, shells, session_manager))
}

async fn handle_socket(
    mut socket: WebSocket,
    sessions: Arc<Mutex<HashMap<String, mpsc::UnboundedSender<String>>>>,
    shells: Arc<Mutex<HashMap<String, Arc<AsyncMutex<PersistentShell>>>>>,
    session_manager: Arc<SessionManager>,
) {
    // Get the eternal session
    let eternal_session = session_manager.get_or_create_session().await;
    let session_id = eternal_session.read().id.to_string();
    
    // Send session ID to client
    let session_msg = WsMessage::Session {
        id: session_id.clone(),
    };
    
    if let Ok(json) = serde_json::to_string(&session_msg) {
        let _ = socket.send(Message::Text(json)).await;
    }
    
    // Create channel for this session
    let (tx, mut rx) = mpsc::unbounded_channel::<String>();
    {
        let mut sessions_guard = sessions.lock();
        sessions_guard.insert(session_id.clone(), tx.clone());
    }
    
    // Create persistent shell for this session
    let shell = match create_shell_session(tx.clone()).await {
        Ok(shell) => Arc::new(AsyncMutex::new(shell)),
        Err(e) => {
            let _ = socket.send(Message::Text(
                serde_json::to_string(&WsMessage::Error { 
                    content: format!("Failed to create shell: {}", e) 
                }).unwrap()
            )).await;
            return;
        }
    };
    
    {
        let mut shells_guard = shells.lock();
        shells_guard.insert(session_id.clone(), shell.clone());
    }
    
    // Send welcome message
    let welcome_msg = WsMessage::Output {
        content: format!("Welcome to Quantum Terminal\nSession: {}\nType 'help' for commands", session_id),
    };
    
    if let Ok(json) = serde_json::to_string(&welcome_msg) {
        let _ = socket.send(Message::Text(json)).await;
    }
    
    // Handle incoming messages and output
    loop {
        tokio::select! {
            // Handle output from commands
            Some(output) = rx.recv() => {
                let msg = WsMessage::Output { content: output };
                if let Ok(json) = serde_json::to_string(&msg) {
                    if socket.send(Message::Text(json)).await.is_err() {
                        break;
                    }
                }
            }
            // Handle incoming WebSocket messages
            result = socket.recv() => {
                match result {
                    Some(Ok(Message::Text(text))) => {
                        if let Ok(ws_msg) = serde_json::from_str::<WsMessage>(&text) {
                            match ws_msg {
                                WsMessage::Command { content } => {
                                    // Add to session history
                                    eternal_session.write().add_to_history(content.clone());
                                    handle_command(&mut socket, &content, tx.clone(), shell.clone(), eternal_session.clone()).await;
                                }
                                _ => {}
                            }
                        }
                    }
                    Some(Ok(Message::Close(_))) => break,
                    None => break,
                    _ => {}
                }
            }
        }
    }
    
    // Clean up session on disconnect
    {
        let mut sessions_guard = sessions.lock();
        sessions_guard.remove(&session_id);
    }
    {
        let mut shells_guard = shells.lock();
        shells_guard.remove(&session_id);
    }
}

async fn handle_command(
    socket: &mut WebSocket, 
    command: &str,
    output_tx: mpsc::UnboundedSender<String>,
    shell: Arc<AsyncMutex<PersistentShell>>,
    session: Arc<parking_lot::RwLock<quantum_terminal::session::EternalSession>>,
) {
    let response = match command.trim() {
        "help" => {
            WsMessage::Output {
                content: "Available commands:\n  help     - Show this help\n  history  - Show command history\n  clear    - Clear terminal\n  echo     - Echo a message\n  date     - Show current date\n  uptime   - Show system uptime\n  Most standard Linux commands are supported!".to_string(),
            }
        }
        "history" => {
            let history = session.read().history.clone();
            let mut content = String::from("Command History:\n");
            for (i, cmd) in history.iter().enumerate().rev().take(20) {
                content.push_str(&format!("  {} {}", history.len() - i, cmd));
                content.push('\n');
            }
            WsMessage::Output { content }
        }
        "clear" => {
            return; // Client will handle clearing
        }
        cmd if is_safe_command(cmd) => {
            // Execute command in persistent shell
            let shell_clone = shell.clone();
            let cmd_owned = command.to_string();
            let tx_clone = output_tx.clone();
            tokio::spawn(async move {
                let mut shell_guard = shell_clone.lock().await;
                if let Err(e) = shell_guard.execute_command(&cmd_owned, tx_clone.clone()).await {
                    let _ = tx_clone.send(format!("Error executing command: {}", e));
                }
            });
            session.write().touch();
            return; // Don't send immediate response
        }
        cmd if cmd.starts_with("echo ") => {
            let message = cmd.strip_prefix("echo ").unwrap_or("");
            WsMessage::Output { content: message.to_string() }
        }
        _ => {
            WsMessage::Error {
                content: format!("Unknown command: {}", command),
            }
        }
    };
    
    if let Ok(json) = serde_json::to_string(&response) {
        let _ = socket.send(Message::Text(json)).await;
    }
}

fn is_safe_command(cmd: &str) -> bool {
    // Dangerous commands that should never be allowed
    let dangerous_commands = [
        "rm", "dd", "mkfs", "format", "fdisk", "parted",
        "shutdown", "reboot", "halt", "poweroff", "init",
        "kill", "killall", "pkill", "systemctl", "service",
        "iptables", "ip6tables", "ufw", "firewall-cmd",
        "userdel", "groupdel", "passwd", "chpasswd",
        "crontab", "at", "batch",
        ":(){ :|:& };:", // Fork bomb
    ];
    
    let cmd_parts: Vec<&str> = cmd.split_whitespace().collect();
    if cmd_parts.is_empty() {
        return false;
    }
    
    let base_cmd = cmd_parts[0];
    
    // Block dangerous commands
    if dangerous_commands.contains(&base_cmd) {
        return false;
    }
    
    // Block dangerous patterns
    if cmd.contains("rm -rf") || cmd.contains("rm -fr") || 
       cmd.contains("> /dev/") || cmd.contains("dd if=") ||
       cmd.contains(":()"){ 
        return false;
    }
    
    // Allow piped commands if all parts are safe
    if cmd.contains('|') {
        return cmd.split('|')
            .all(|part| is_safe_command(part.trim()));
    }
    
    // Allow commands with redirections if safe
    if cmd.contains('>') || cmd.contains('<') {
        // Don't allow writing to system files
        if cmd.contains("> /etc") || cmd.contains("> /sys") || 
           cmd.contains("> /proc") || cmd.contains("> /dev") {
            return false;
        }
        let parts: Vec<&str> = cmd.split(&['>', '<'][..]).collect();
        return parts.iter().all(|part| is_safe_command(part.trim()));
    }
    
    // Allow everything else by default
    true
}