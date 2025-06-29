//! Persistent shell implementation with proper directory tracking
//!
//! Maintains a single bash process per session for stateful command execution

use std::process::Stdio;
use tokio::process::{Child, Command};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::sync::mpsc;
use std::sync::Arc;
use tokio::sync::Mutex;

pub struct PersistentShell {
    child: Child,
    stdin: tokio::process::ChildStdin,
    current_dir: Arc<Mutex<String>>,
}

impl PersistentShell {
    pub async fn new() -> Result<Self, std::io::Error> {
        let mut child = Command::new("/bin/bash")
            .arg("-i")  // Interactive mode
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .env("PS1", "") // Disable prompt
            .spawn()?;
        
        let stdin = child.stdin.take().expect("Failed to get stdin");
        let current_dir = Arc::new(Mutex::new(
            std::env::current_dir()?.to_string_lossy().to_string()
        ));
        
        Ok(Self {
            child,
            stdin,
            current_dir,
        })
    }
    
    pub async fn execute_command(
        &mut self,
        command: &str,
        output_tx: mpsc::UnboundedSender<String>,
    ) -> Result<(), std::io::Error> {
        // Special handling for cd command
        if command.trim().starts_with("cd ") || command.trim() == "cd" {
            self.handle_cd_command(command, output_tx.clone()).await?;
        } else {
            // Execute command with output markers
            let wrapped_command = format!(
                "echo '<<<QTERM_START>>>'; {}; echo '<<<QTERM_END:$?>>>'",
                command
            );
            
            self.stdin.write_all(wrapped_command.as_bytes()).await?;
            self.stdin.write_all(b"\n").await?;
            self.stdin.flush().await?;
        }
        
        Ok(())
    }
    
    async fn handle_cd_command(
        &mut self,
        command: &str,
        output_tx: mpsc::UnboundedSender<String>,
    ) -> Result<(), std::io::Error> {
        // Execute cd and get new directory
        let cd_check = format!(
            "{}; echo '<<<QTERM_DIR>>>'$(pwd)'<<<QTERM_DIR_END>>>'",
            command
        );
        
        self.stdin.write_all(cd_check.as_bytes()).await?;
        self.stdin.write_all(b"\n").await?;
        self.stdin.flush().await?;
        
        Ok(())
    }
    
    pub async fn get_current_dir(&self) -> String {
        self.current_dir.lock().await.clone()
    }
    
    pub async fn update_current_dir(&self, dir: String) {
        *self.current_dir.lock().await = dir;
    }
}

pub async fn create_shell_session(
    output_tx: mpsc::UnboundedSender<String>,
) -> Result<PersistentShell, std::io::Error> {
    let mut shell = PersistentShell::new().await?;
    
    // Start output reader tasks
    if let Some(stdout) = shell.child.stdout.take() {
        let tx = output_tx.clone();
        let current_dir = shell.current_dir.clone();
        
        tokio::spawn(async move {
            let reader = BufReader::new(stdout);
            let mut lines = reader.lines();
            let mut in_output = false;
            
            while let Ok(Some(line)) = lines.next_line().await {
                // Parse markers
                if line.contains("<<<QTERM_START>>>") {
                    in_output = true;
                    continue;
                } else if line.contains("<<<QTERM_END:") {
                    in_output = false;
                    // Extract exit code if needed
                    continue;
                } else if line.contains("<<<QTERM_DIR>>>") {
                    // Extract directory
                    if let Some(start) = line.find("<<<QTERM_DIR>>>") {
                        if let Some(end) = line.find("<<<QTERM_DIR_END>>>") {
                            let dir = line[start + 15..end].to_string();
                            *current_dir.lock().await = dir.clone();
                            let _ = tx.send(format!("Changed to: {}", dir));
                        }
                    }
                    continue;
                }
                
                // Send regular output
                if in_output || (!line.contains("<<<QTERM") && !line.is_empty()) {
                    let _ = tx.send(line);
                }
            }
        });
    }
    
    if let Some(stderr) = shell.child.stderr.take() {
        let tx = output_tx.clone();
        
        tokio::spawn(async move {
            let reader = BufReader::new(stderr);
            let mut lines = reader.lines();
            
            while let Ok(Some(line)) = lines.next_line().await {
                if !line.contains("<<<QTERM") {
                    let _ = tx.send(line);
                }
            }
        });
    }
    
    Ok(shell)
}