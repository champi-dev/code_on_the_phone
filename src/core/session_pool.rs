//! Lock-free session pool with O(1) allocation and deallocation
//!
//! Pre-allocates sessions to guarantee O(1) operations without heap allocation
//! during runtime.

use crossbeam::queue::ArrayQueue;
use std::sync::Arc;
use uuid::Uuid;

/// Session object with pre-allocated buffers
pub struct Session {
    pub id: Uuid,
    pub created_at: u64,
    pub command_buffer: Vec<u8>,
    pub output_buffer: Vec<u8>,
}

impl Session {
    /// Creates a new session with pre-allocated buffers
    fn new(buffer_size: usize) -> Self {
        let mut command_buffer = Vec::with_capacity(buffer_size);
        let mut output_buffer = Vec::with_capacity(buffer_size);
        
        // Pre-fault pages for true O(1) access
        command_buffer.resize(buffer_size, 0);
        output_buffer.resize(buffer_size, 0);
        command_buffer.clear();
        output_buffer.clear();
        
        Self {
            id: Uuid::nil(),
            created_at: 0,
            command_buffer,
            output_buffer,
        }
    }
    
    /// Resets session for reuse - O(1) operation
    #[inline]
    fn reset(&mut self) {
        self.id = Uuid::nil();
        self.created_at = 0;
        self.command_buffer.clear();
        self.output_buffer.clear();
    }
}

/// Lock-free session pool with guaranteed O(1) operations
pub struct SessionPool {
    pool: Arc<ArrayQueue<Box<Session>>>,
    buffer_size: usize,
}

impl SessionPool {
    /// Creates a new session pool with pre-allocated sessions
    pub fn new(capacity: usize, buffer_size: usize) -> Self {
        let pool = Arc::new(ArrayQueue::new(capacity));
        
        // Pre-allocate all sessions
        for _ in 0..capacity {
            let session = Box::new(Session::new(buffer_size));
            let _ = pool.push(session); // Guaranteed to succeed
        }
        
        Self { pool, buffer_size }
    }
    
    /// Acquires a session from the pool - O(1) operation
    #[inline]
    pub fn acquire(&self) -> Option<Box<Session>> {
        self.pool.pop()
    }
    
    /// Returns a session to the pool - O(1) operation  
    #[inline]
    pub fn release(&self, mut session: Box<Session>) {
        session.reset();
        let _ = self.pool.push(session); // Ignore if pool is full
    }
    
    /// Returns the number of available sessions - O(1) operation
    #[inline]
    pub fn available(&self) -> usize {
        self.pool.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_session_pool_allocation() {
        let pool = SessionPool::new(10, 1024);
        assert_eq!(pool.available(), 10);
        
        let session = pool.acquire().expect("Should acquire session");
        assert_eq!(pool.available(), 9);
        
        pool.release(session);
        assert_eq!(pool.available(), 10);
    }
}