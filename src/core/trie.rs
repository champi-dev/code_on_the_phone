//! Trie implementation for O(log k) command completion
//!
//! Provides efficient prefix-based search and completion for commands.

use std::collections::HashMap;

/// Trie node for command storage
pub struct TrieNode<T> {
    /// Child nodes indexed by character
    children: HashMap<char, Box<TrieNode<T>>>,
    /// Value stored at this node (if terminal)
    value: Option<T>,
    /// Whether this node represents a complete command
    is_terminal: bool,
}

impl<T> TrieNode<T> {
    /// Creates a new empty trie node
    pub fn new() -> Self {
        Self {
            children: HashMap::new(),
            value: None,
            is_terminal: false,
        }
    }
}

/// Trie data structure for O(log k) operations
pub struct Trie<T> {
    root: TrieNode<T>,
    size: usize,
}

impl<T> Trie<T> {
    /// Creates a new empty trie
    pub fn new() -> Self {
        Self {
            root: TrieNode::new(),
            size: 0,
        }
    }
    
    /// Inserts a key-value pair - O(k) where k is key length
    pub fn insert(&mut self, key: &str, value: T) {
        let mut node = &mut self.root;
        
        for ch in key.chars() {
            node = node.children
                .entry(ch)
                .or_insert_with(|| Box::new(TrieNode::new()));
        }
        
        if !node.is_terminal {
            self.size += 1;
        }
        
        node.value = Some(value);
        node.is_terminal = true;
    }
    
    /// Searches for a key - O(k) where k is key length
    pub fn get(&self, key: &str) -> Option<&T> {
        let mut node = &self.root;
        
        for ch in key.chars() {
            match node.children.get(&ch) {
                Some(child) => node = child,
                None => return None,
            }
        }
        
        if node.is_terminal {
            node.value.as_ref()
        } else {
            None
        }
    }
    
    /// Finds all keys with given prefix - O(k + m) where m is number of matches
    pub fn prefix_search(&self, prefix: &str) -> Vec<String> {
        let mut node = &self.root;
        let mut results = Vec::new();
        
        // Navigate to prefix node
        for ch in prefix.chars() {
            match node.children.get(&ch) {
                Some(child) => node = child,
                None => return results,
            }
        }
        
        // Collect all completions
        self.collect_completions(node, prefix.to_string(), &mut results);
        results
    }
    
    /// Helper to collect all completions from a node
    fn collect_completions(&self, node: &TrieNode<T>, prefix: String, results: &mut Vec<String>) {
        if node.is_terminal {
            results.push(prefix.clone());
        }
        
        for (ch, child) in &node.children {
            let mut new_prefix = prefix.clone();
            new_prefix.push(*ch);
            self.collect_completions(child, new_prefix, results);
        }
    }
    
    /// Returns the number of keys in the trie
    pub fn len(&self) -> usize {
        self.size
    }
    
    /// Checks if the trie is empty
    pub fn is_empty(&self) -> bool {
        self.size == 0
    }
}

impl<T> Default for Trie<T> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_trie_operations() {
        let mut trie = Trie::new();
        
        trie.insert("ls", 1);
        trie.insert("lst", 2);
        trie.insert("list", 3);
        
        assert_eq!(trie.get("ls"), Some(&1));
        assert_eq!(trie.get("lst"), Some(&2));
        assert_eq!(trie.get("list"), Some(&3));
        assert_eq!(trie.get("l"), None);
        
        let completions = trie.prefix_search("ls");
        assert_eq!(completions.len(), 2);
        assert!(completions.contains(&"ls".to_string()));
        assert!(completions.contains(&"lst".to_string()));
    }
}