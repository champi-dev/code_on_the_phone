//! Core data structures with O(1) and O(log n) complexity guarantees
//!
//! This module implements the fundamental data structures that power
//! Quantum Terminal's elite performance.

pub mod session_pool;
pub mod command_history;
pub mod trie;
pub mod metrics;

use std::sync::atomic::{AtomicU64, Ordering};

/// Unique identifier generation with O(1) complexity
pub struct IdGenerator {
    counter: AtomicU64,
}

impl IdGenerator {
    pub const fn new() -> Self {
        Self {
            counter: AtomicU64::new(1),
        }
    }
    
    /// Generates a unique ID in O(1) time
    #[inline]
    pub fn next(&self) -> u64 {
        self.counter.fetch_add(1, Ordering::Relaxed)
    }
}

impl Default for IdGenerator {
    fn default() -> Self {
        Self::new()
    }
}

/// Cache-aligned data structure for optimal performance
#[repr(align(64))]
pub struct CacheAligned<T>(pub T);

impl<T> std::ops::Deref for CacheAligned<T> {
    type Target = T;
    
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> std::ops::DerefMut for CacheAligned<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}