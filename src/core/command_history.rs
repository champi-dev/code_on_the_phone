//! Command history with O(1) access and deduplication
//!
//! Uses a circular buffer with hash-based indexing for constant-time operations
//! and a bloom filter for fast existence checks.

use std::collections::HashMap;
use std::hash::{Hash, Hasher};
use std::collections::hash_map::DefaultHasher;

/// Command entry in history
#[derive(Clone, Debug)]
pub struct Command {
    pub text: String,
    pub timestamp: u64,
    pub exit_code: i32,
}

/// Bloom filter for O(1) existence checks
pub struct BloomFilter {
    bits: Vec<u64>,
    size: usize,
}

impl BloomFilter {
    /// Creates a new bloom filter with optimal size
    pub fn new(expected_items: usize) -> Self {
        // Optimal size: -n*ln(p)/(ln(2)^2) where p=0.01 (1% false positive)
        let size = (expected_items as f64 * 10.0 / 0.48).ceil() as usize;
        let words = (size + 63) / 64;
        
        Self {
            bits: vec![0; words],
            size,
        }
    }
    
    /// Adds an item to the bloom filter - O(1)
    #[inline]
    pub fn insert(&mut self, hash: u64) {
        // Use 3 hash functions for optimal false positive rate
        for i in 0..3 {
            let bit_idx = self.hash_to_index(hash, i);
            let word_idx = bit_idx / 64;
            let bit_offset = bit_idx % 64;
            self.bits[word_idx] |= 1u64 << bit_offset;
        }
    }
    
    /// Checks if an item might exist - O(1)
    #[inline]
    pub fn contains(&self, hash: u64) -> bool {
        for i in 0..3 {
            let bit_idx = self.hash_to_index(hash, i);
            let word_idx = bit_idx / 64;
            let bit_offset = bit_idx % 64;
            if self.bits[word_idx] & (1u64 << bit_offset) == 0 {
                return false;
            }
        }
        true
    }
    
    #[inline]
    fn hash_to_index(&self, hash: u64, seed: u64) -> usize {
        // Double hashing for independent hash functions
        let h1 = hash;
        let h2 = hash.rotate_left(32);
        ((h1.wrapping_add(seed.wrapping_mul(h2))) as usize) % self.size
    }
}

/// Command history with O(1) operations
pub struct CommandHistory {
    /// Circular buffer for commands
    buffer: Vec<Option<Command>>,
    /// Current write position
    write_pos: usize,
    /// Total commands written
    total_written: usize,
    /// Hash index for deduplication
    dedup_index: HashMap<u64, usize>,
    /// Bloom filter for fast existence check
    bloom: BloomFilter,
    /// Capacity (power of 2)
    capacity: usize,
}

impl CommandHistory {
    /// Creates a new command history with given capacity
    pub fn new(capacity: usize) -> Self {
        // Ensure capacity is power of 2 for bit manipulation
        let capacity = capacity.next_power_of_two();
        
        Self {
            buffer: vec![None; capacity],
            write_pos: 0,
            total_written: 0,
            dedup_index: HashMap::with_capacity(capacity),
            bloom: BloomFilter::new(capacity),
            capacity,
        }
    }
    
    /// Adds a command to history - O(1) amortized
    pub fn add(&mut self, command: Command) -> bool {
        let hash = self.hash_command(&command.text);
        
        // Fast existence check with bloom filter
        if self.bloom.contains(hash) {
            // Check actual existence in dedup index
            if let Some(&existing_pos) = self.dedup_index.get(&hash) {
                // Update timestamp of existing command
                if let Some(ref mut existing) = self.buffer[existing_pos] {
                    existing.timestamp = command.timestamp;
                    return false; // Not a new command
                }
            }
        }
        
        // Add new command
        let pos = self.write_pos;
        
        // Remove old entry from dedup index if overwriting
        if let Some(ref old_command) = self.buffer[pos] {
            let old_hash = self.hash_command(&old_command.text);
            self.dedup_index.remove(&old_hash);
        }
        
        // Insert new command
        self.buffer[pos] = Some(command);
        self.dedup_index.insert(hash, pos);
        self.bloom.insert(hash);
        
        // Update write position using bit manipulation (faster than modulo)
        self.write_pos = (self.write_pos + 1) & (self.capacity - 1);
        self.total_written += 1;
        
        true
    }
    
    /// Gets the most recent n commands - O(n)
    pub fn recent(&self, n: usize) -> Vec<&Command> {
        let mut result = Vec::with_capacity(n.min(self.capacity));
        let start = if self.total_written < self.capacity {
            0
        } else {
            self.write_pos
        };
        
        for i in 0..n.min(self.total_written.min(self.capacity)) {
            let pos = (start + self.capacity - 1 - i) & (self.capacity - 1);
            if let Some(ref cmd) = self.buffer[pos] {
                result.push(cmd);
            }
        }
        
        result
    }
    
    /// Searches for commands containing text - O(n) where n is history size
    pub fn search(&self, query: &str) -> Vec<&Command> {
        self.buffer
            .iter()
            .filter_map(|opt| opt.as_ref())
            .filter(|cmd| cmd.text.contains(query))
            .collect()
    }
    
    #[inline]
    fn hash_command(&self, text: &str) -> u64 {
        let mut hasher = DefaultHasher::new();
        text.hash(&mut hasher);
        hasher.finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_command_history() {
        let mut history = CommandHistory::new(4);
        
        // Add commands
        assert!(history.add(Command {
            text: "ls".to_string(),
            timestamp: 1,
            exit_code: 0,
        }));
        
        // Duplicate should not be added
        assert!(!history.add(Command {
            text: "ls".to_string(),
            timestamp: 2,
            exit_code: 0,
        }));
        
        // Test recent commands
        let recent = history.recent(1);
        assert_eq!(recent.len(), 1);
        assert_eq!(recent[0].text, "ls");
    }
}