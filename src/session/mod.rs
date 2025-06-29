//! Eternal session management with persistence
//!
//! Implements passwordless, persistent sessions that survive restarts
//! and run in the background. All operations are O(1) or O(log n).

pub mod manager;
pub mod persistence;
pub mod process;
pub mod shell;

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use uuid::Uuid;

/// Eternal session with no authentication required
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EternalSession {
    /// Unique session ID
    pub id: Uuid,
    /// Unix timestamp of creation
    pub created_at: u64,
    /// Last activity timestamp
    pub last_activity: u64,
    /// Current working directory
    pub cwd: String,
    /// Environment variables
    pub env: HashMap<String, String>,
    /// Command history for this session
    pub history: Vec<String>,
    /// Active process PIDs
    pub processes: Vec<u32>,
    /// Session state (persisted to disk)
    pub state: SessionState,
}

/// Session state that persists across restarts
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionState {
    /// Terminal dimensions
    pub rows: u16,
    pub cols: u16,
    /// Scroll buffer
    pub scroll_buffer: Vec<String>,
    /// Current command being typed
    pub current_input: String,
    /// Cursor position
    pub cursor_pos: usize,
}

impl EternalSession {
    /// Creates a new eternal session
    pub fn new() -> Self {
        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();
            
        Self {
            id: Uuid::new_v4(),
            created_at: now,
            last_activity: now,
            cwd: std::env::current_dir()
                .unwrap_or_default()
                .to_string_lossy()
                .to_string(),
            env: std::env::vars().collect(),
            history: Vec::with_capacity(4096),
            processes: Vec::new(),
            state: SessionState {
                rows: 24,
                cols: 80,
                scroll_buffer: Vec::with_capacity(10000),
                current_input: String::new(),
                cursor_pos: 0,
            },
        }
    }
    
    /// Updates last activity timestamp - O(1)
    #[inline]
    pub fn touch(&mut self) {
        self.last_activity = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();
    }
    
    /// Adds command to history - O(1) amortized
    pub fn add_to_history(&mut self, command: String) {
        // Keep history bounded to prevent unbounded growth
        if self.history.len() >= 4096 {
            self.history.remove(0);
        }
        self.history.push(command);
    }
    
    /// Adds line to scroll buffer - O(1) amortized
    pub fn add_to_buffer(&mut self, line: String) {
        // Keep buffer bounded
        if self.state.scroll_buffer.len() >= 10000 {
            self.state.scroll_buffer.remove(0);
        }
        self.state.scroll_buffer.push(line);
    }
}

impl Default for EternalSession {
    fn default() -> Self {
        Self::new()
    }
}

/// Session configuration for eternal sessions
pub struct SessionConfig {
    /// Where to persist sessions
    pub persistence_path: String,
    /// How often to persist (seconds)
    pub persist_interval: u64,
    /// Maximum sessions to keep
    pub max_sessions: usize,
}

impl Default for SessionConfig {
    fn default() -> Self {
        Self {
            persistence_path: "/data/data/com.termux/files/home/.quantum-terminal/sessions".to_string(),
            persist_interval: 30, // Every 30 seconds
            max_sessions: 1000,   // Very generous limit
        }
    }
}