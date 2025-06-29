//! Lock-free metrics collection with O(1) updates
//!
//! Uses atomic operations for zero-contention statistics gathering.

use std::sync::atomic::{AtomicU64, AtomicUsize, Ordering};

/// Atomic metrics for zero-contention updates
pub struct Metrics {
    /// Total commands executed
    pub commands_executed: AtomicU64,
    /// Total bytes received
    pub bytes_received: AtomicU64,
    /// Total bytes sent
    pub bytes_sent: AtomicU64,
    /// Active sessions
    pub active_sessions: AtomicUsize,
    /// Total sessions created
    pub total_sessions: AtomicU64,
    /// Commands per second (moving average)
    pub commands_per_second: AtomicU64,
}

impl Metrics {
    /// Creates new metrics instance
    pub const fn new() -> Self {
        Self {
            commands_executed: AtomicU64::new(0),
            bytes_received: AtomicU64::new(0),
            bytes_sent: AtomicU64::new(0),
            active_sessions: AtomicUsize::new(0),
            total_sessions: AtomicU64::new(0),
            commands_per_second: AtomicU64::new(0),
        }
    }
    
    /// Records a command execution - O(1)
    #[inline]
    pub fn record_command(&self) {
        self.commands_executed.fetch_add(1, Ordering::Relaxed);
    }
    
    /// Records bytes received - O(1)
    #[inline]
    pub fn record_bytes_received(&self, bytes: u64) {
        self.bytes_received.fetch_add(bytes, Ordering::Relaxed);
    }
    
    /// Records bytes sent - O(1)
    #[inline]
    pub fn record_bytes_sent(&self, bytes: u64) {
        self.bytes_sent.fetch_add(bytes, Ordering::Relaxed);
    }
    
    /// Increments active sessions - O(1)
    #[inline]
    pub fn session_started(&self) {
        self.active_sessions.fetch_add(1, Ordering::Relaxed);
        self.total_sessions.fetch_add(1, Ordering::Relaxed);
    }
    
    /// Decrements active sessions - O(1)
    #[inline]
    pub fn session_ended(&self) {
        self.active_sessions.fetch_sub(1, Ordering::Relaxed);
    }
    
    /// Updates commands per second - O(1)
    #[inline]
    pub fn update_commands_per_second(&self, cps: u64) {
        self.commands_per_second.store(cps, Ordering::Relaxed);
    }
    
    /// Gets current metrics snapshot - O(1)
    pub fn snapshot(&self) -> MetricsSnapshot {
        MetricsSnapshot {
            commands_executed: self.commands_executed.load(Ordering::Relaxed),
            bytes_received: self.bytes_received.load(Ordering::Relaxed),
            bytes_sent: self.bytes_sent.load(Ordering::Relaxed),
            active_sessions: self.active_sessions.load(Ordering::Relaxed),
            total_sessions: self.total_sessions.load(Ordering::Relaxed),
            commands_per_second: self.commands_per_second.load(Ordering::Relaxed),
        }
    }
}

impl Default for Metrics {
    fn default() -> Self {
        Self::new()
    }
}

/// Snapshot of metrics at a point in time
#[derive(Debug, Clone)]
pub struct MetricsSnapshot {
    pub commands_executed: u64,
    pub bytes_received: u64,
    pub bytes_sent: u64,
    pub active_sessions: usize,
    pub total_sessions: u64,
    pub commands_per_second: u64,
}