//! Process management for eternal sessions
//!
//! Handles background process execution with proper lifecycle management.

use nix::sys::signal::{kill, Signal};
use nix::unistd::Pid;
use std::process::Stdio;
use tokio::process::{Child, Command};
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::sync::mpsc;

/// Process handle for background execution
pub struct ProcessHandle {
    pub pid: u32,
    pub child: Child,
}

/// Spawns a shell process for the session
pub async fn spawn_shell() -> Result<ProcessHandle, std::io::Error> {
    let child = Command::new("/bin/bash")
        .arg("--norc") // Skip RC files for speed
        .arg("-i")      // Interactive mode
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()?;
    
    let pid = child.id().unwrap_or(0);
    
    Ok(ProcessHandle { pid, child })
}

/// Executes a command in the background
pub async fn execute_command(
    command: &str,
    output_tx: mpsc::UnboundedSender<String>
) -> Result<i32, std::io::Error> {
    let mut child = Command::new("/bin/bash")
        .arg("-c")
        .arg(command)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()?;
    
    // Stream output as it arrives
    if let Some(stdout) = child.stdout.take() {
        let reader = BufReader::new(stdout);
        let tx = output_tx.clone();
        
        tokio::spawn(async move {
            let mut lines = reader.lines();
            while let Ok(Some(line)) = lines.next_line().await {
                let _ = tx.send(line);
            }
        });
    }
    
    if let Some(stderr) = child.stderr.take() {
        let reader = BufReader::new(stderr);
        
        tokio::spawn(async move {
            let mut lines = reader.lines();
            while let Ok(Some(line)) = lines.next_line().await {
                let _ = output_tx.send(format!("ERR: {}", line));
            }
        });
    }
    
    // Wait for completion
    let status = child.wait().await?;
    Ok(status.code().unwrap_or(-1))
}

/// Kills a process gracefully
pub fn kill_process(pid: u32) -> Result<(), nix::Error> {
    let pid = Pid::from_raw(pid as i32);
    
    // Try SIGTERM first
    kill(pid, Signal::SIGTERM)?;
    
    // Give it time to clean up
    std::thread::sleep(std::time::Duration::from_millis(100));
    
    // Force kill if still alive
    if let Err(_) = kill(pid, Signal::SIGKILL) {
        // Process already dead, that's fine
    }
    
    Ok(())
}

/// Checks if a process is still alive
pub fn is_process_alive(pid: u32) -> bool {
    let pid = Pid::from_raw(pid as i32);
    kill(pid, None).is_ok()
}