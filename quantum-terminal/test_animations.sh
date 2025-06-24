#!/bin/bash

# Test script for quantum terminal animations
# This script demonstrates all the easter egg animations

echo "=== Quantum Terminal Animation Test ==="
echo

# Function to pause and show effect
show_effect() {
    echo "[$1]"
    sleep 0.5
    eval "$2"
    sleep 2
    echo
}

# Matrix rain effect
show_effect "Matrix Rain Animation" "ls -la"

# Wormhole portal effect
show_effect "Wormhole Portal Animation" "cd /tmp"

# DNA helix effect
show_effect "DNA Helix Animation" "git status"

# Glitch text effect
show_effect "Glitch Text Animation" "sudo whoami"

# Particle fountain effect
show_effect "Particle Fountain Animation" "make clean"

# Time warp effect
show_effect "Time Warp Animation" "history"

# Neural network effect
show_effect "Neural Network Animation" "python --version"

# Cosmic rays effect
show_effect "Cosmic Rays Animation" "vim test.txt"

# Quantum tunnel effect
show_effect "Quantum Tunnel Animation" "echo 'ssh user@host'"

# Quantum explosion effect (be careful!)
echo "=== WARNING: Quantum Explosion Animation ==="
echo "This simulates 'rm -rf' but doesn't actually run it!"
echo "[Quantum Explosion Animation]"
echo "rm -rf /tmp/test"  # Just echo, don't run!
sleep 3

echo
echo "=== Animation Test Complete ==="
echo "All quantum animations have been demonstrated!"