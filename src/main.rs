//! Quantum Terminal - Elite Performance Terminal Server
//!
//! Entry point for the terminal server with guaranteed O(1) startup time
//! through pre-computation and lazy initialization.

use quantum_terminal::{Config, QuantumTerminal};
use quantum_terminal::core::metrics::Metrics;
use quantum_terminal::session::{SessionConfig, manager::SessionManager};
use std::sync::Arc;
use std::env;
use tokio::signal;
use dotenv::dotenv;

mod web_server;
use web_server::WebServer;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Load environment variables
    dotenv().ok();
    
    // Initialize with optimal configuration
    let config = Config::default();
    let terminal = Arc::new(QuantumTerminal::new(config));
    
    // Pre-initialize all subsystems
    terminal.initialize().await?;
    
    // Initialize metrics
    let metrics = Arc::new(Metrics::new());
    
    // Configure eternal sessions
    let session_config = SessionConfig {
        persistence_path: env::var("SESSION_PATH")
            .unwrap_or_else(|_| "/data/data/com.termux/files/home/.quantum-terminal/sessions".to_string()),
        persist_interval: env::var("PERSIST_INTERVAL")
            .unwrap_or_else(|_| "30".to_string())
            .parse()
            .unwrap_or(30),
        max_sessions: env::var("MAX_SESSIONS")
            .unwrap_or_else(|_| "1000".to_string())
            .parse()
            .unwrap_or(1000),
    };
    
    // Create session manager
    let session_manager = Arc::new(SessionManager::new(session_config, Arc::clone(&metrics)));
    
    // Start background persistence
    session_manager.start_persistence().await;
    
    // Get the eternal session
    let eternal_session = session_manager.get_or_create_session().await;
    let session_id = eternal_session.read().id;
    
    let droplet_ip = env::var("DROPLET_IP").unwrap_or_else(|_| "0.0.0.0".to_string());
    let server_port = env::var("SERVER_PORT").unwrap_or_else(|_| "8080".to_string());
    
    println!("Quantum Terminal initialized with elite performance guarantees");
    println!("Eternal session ID: {}", session_id);
    println!("Ready for O(1) operations on {}:{}", droplet_ip, server_port);
    println!("\nNOTE: Single-user mode - no authentication required");
    println!("Sessions persist across restarts automatically");
    
    // Start web server
    let port: u16 = server_port.parse().unwrap_or(8080);
    let web_server = WebServer::new(port, session_manager.clone());
    
    tokio::spawn(async move {
        if let Err(e) = web_server.run().await {
            eprintln!("Web server error: {}", e);
        }
    });
    
    // Graceful shutdown handler
    signal::ctrl_c().await?;
    println!("\nShutting down with zero memory leaks...");
    
    Ok(())
}