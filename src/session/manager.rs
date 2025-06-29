//! Eternal session manager with O(1) operations
//!
//! Manages all active sessions with automatic persistence and recovery.

use super::{EternalSession, SessionConfig};
use crate::core::metrics::Metrics;
use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;
use tokio::time::{interval, Duration};
use uuid::Uuid;

/// Manager for eternal sessions with background persistence
pub struct SessionManager {
    /// Active sessions - O(1) lookup
    sessions: Arc<RwLock<HashMap<Uuid, Arc<RwLock<EternalSession>>>>>,
    /// Configuration
    config: Arc<SessionConfig>,
    /// Global metrics
    metrics: Arc<Metrics>,
}

impl SessionManager {
    /// Creates a new session manager
    pub fn new(config: SessionConfig, metrics: Arc<Metrics>) -> Self {
        Self {
            sessions: Arc::new(RwLock::new(HashMap::with_capacity(config.max_sessions))),
            config: Arc::new(config),
            metrics,
        }
    }
    
    /// Starts background persistence task
    pub async fn start_persistence(&self) {
        let sessions = Arc::clone(&self.sessions);
        let config = Arc::clone(&self.config);
        let persistence_path = config.persistence_path.clone();
        
        tokio::spawn(async move {
            let mut interval = interval(Duration::from_secs(config.persist_interval));
            
            loop {
                interval.tick().await;
                
                // Collect sessions to persist
                let sessions_to_persist: Vec<(Uuid, EternalSession)> = {
                    let sessions_guard = sessions.read();
                    sessions_guard.iter()
                        .map(|(id, session)| (*id, session.read().clone()))
                        .collect()
                };
                
                // Persist outside the lock
                for (id, session) in sessions_to_persist {
                    if let Err(e) = super::persistence::save_session(&persistence_path, &session).await {
                        eprintln!("Failed to persist session {}: {}", id, e);
                    }
                }
            }
        });
    }
    
    /// Creates or retrieves the eternal session - O(1)
    pub async fn get_or_create_session(&self) -> Arc<RwLock<EternalSession>> {
        // For single-user mode, always use the same session ID
        let eternal_id = Uuid::from_u128(0xDEADBEEF_CAFE_BABE_DEAD_BEEFCAFEBABE);
        
        // Check if session exists
        {
            let sessions = self.sessions.read();
            if let Some(session) = sessions.get(&eternal_id) {
                // Update activity timestamp
                session.write().touch();
                return Arc::clone(session);
            }
        }
        
        // Session doesn't exist, try to load from disk
        let session = if let Ok(loaded) = super::persistence::load_session(
            &self.config.persistence_path,
            eternal_id
        ).await {
            loaded
        } else {
            // Create new eternal session
            let mut new_session = EternalSession::new();
            new_session.id = eternal_id;
            new_session
        };
        
        let session_arc = Arc::new(RwLock::new(session));
        
        // Insert into sessions map
        {
            let mut sessions = self.sessions.write();
            sessions.insert(eternal_id, Arc::clone(&session_arc));
        }
        
        self.metrics.session_started();
        
        session_arc
    }
    
    /// Gets a specific session by ID - O(1)
    pub fn get_session(&self, id: Uuid) -> Option<Arc<RwLock<EternalSession>>> {
        self.sessions.read().get(&id).cloned()
    }
    
    /// Lists all active sessions - O(n)
    pub fn list_sessions(&self) -> Vec<Uuid> {
        self.sessions.read().keys().copied().collect()
    }
    
    /// Removes a session - O(1)
    pub fn remove_session(&self, id: Uuid) -> bool {
        let removed = self.sessions.write().remove(&id).is_some();
        if removed {
            self.metrics.session_ended();
        }
        removed
    }
    
    /// Gets session count - O(1)
    pub fn session_count(&self) -> usize {
        self.sessions.read().len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[tokio::test]
    async fn test_eternal_session() {
        let metrics = Arc::new(Metrics::new());
        let config = SessionConfig::default();
        let manager = SessionManager::new(config, metrics);
        
        // Get eternal session
        let session1 = manager.get_or_create_session().await;
        let id1 = session1.read().id;
        
        // Get again - should be same session
        let session2 = manager.get_or_create_session().await;
        let id2 = session2.read().id;
        
        assert_eq!(id1, id2);
        assert_eq!(manager.session_count(), 1);
    }
}