//! Session persistence for eternal sessions
//!
//! Saves and loads sessions from disk with binary serialization.

use super::EternalSession;
use tokio::fs;
use uuid::Uuid;

/// Saves a session to disk - O(n) where n is session data size
pub async fn save_session(base_path: &str, session: &EternalSession) -> Result<(), std::io::Error> {
    // Create directory if it doesn't exist
    fs::create_dir_all(base_path).await?;
    
    let file_path = format!("{}/{}.session", base_path, session.id);
    
    // Serialize with bincode for efficiency
    let data = bincode::serialize(session)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?;
    
    // Write atomically with temp file
    let temp_path = format!("{}.tmp", file_path);
    fs::write(&temp_path, data).await?;
    fs::rename(&temp_path, &file_path).await?;
    
    Ok(())
}

/// Loads a session from disk - O(n) where n is session data size
pub async fn load_session(base_path: &str, id: Uuid) -> Result<EternalSession, std::io::Error> {
    let file_path = format!("{}/{}.session", base_path, id);
    
    let data = fs::read(&file_path).await?;
    
    bincode::deserialize(&data)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))
}

/// Lists all saved sessions - O(n) where n is number of files
pub async fn list_saved_sessions(base_path: &str) -> Result<Vec<Uuid>, std::io::Error> {
    let mut sessions = Vec::new();
    
    let mut entries = fs::read_dir(base_path).await?;
    while let Some(entry) = entries.next_entry().await? {
        let path = entry.path();
        if let Some(ext) = path.extension() {
            if ext == "session" {
                if let Some(stem) = path.file_stem() {
                    if let Ok(id) = Uuid::parse_str(&stem.to_string_lossy()) {
                        sessions.push(id);
                    }
                }
            }
        }
    }
    
    Ok(sessions)
}

/// Cleans up old sessions beyond limit - O(n log n)
pub async fn cleanup_old_sessions(base_path: &str, keep: usize) -> Result<(), std::io::Error> {
    let mut sessions = Vec::new();
    
    let mut entries = fs::read_dir(base_path).await?;
    while let Some(entry) = entries.next_entry().await? {
        let metadata = entry.metadata().await?;
        if let Ok(modified) = metadata.modified() {
            sessions.push((modified, entry.path()));
        }
    }
    
    // Sort by modification time (newest first)
    sessions.sort_by(|a, b| b.0.cmp(&a.0));
    
    // Remove old sessions
    for (_, path) in sessions.iter().skip(keep) {
        let _ = fs::remove_file(path).await;
    }
    
    Ok(())
}