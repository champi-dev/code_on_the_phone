#!/bin/bash

echo "=== Local Testing Guide for Rust Web Terminal ==="
echo

# Check if we're in the right directory
if [ ! -f "Cargo.toml" ]; then
    echo "Error: Not in rust-web-terminal directory"
    echo "Please run: cd rust-web-terminal"
    exit 1
fi

echo "1. Installing dependencies..."
echo "   - Installing Rust toolchain..."
if ! command -v rustup &> /dev/null; then
    echo "   Installing rustup..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    source $HOME/.cargo/env
fi

echo "   - Adding wasm32 target..."
rustup target add wasm32-unknown-unknown

echo "   - Installing wasm-pack..."
if ! command -v wasm-pack &> /dev/null; then
    npm install -g wasm-pack || cargo install wasm-pack
fi

echo
echo "2. Building the project..."
wasm-pack build --target web --out-dir pkg --dev

echo
echo "3. Starting local server..."
echo "   Server will run on http://localhost:8080"
echo

# Get local IP address
LOCAL_IP=$(hostname -I | awk '{print $1}' || echo "localhost")

echo "Starting server on 0.0.0.0:8080..."
echo "Access the terminal at:"
echo "  - http://localhost:8080"
echo "  - http://$LOCAL_IP:8080"
echo "  - http://0.0.0.0:8080"
echo

# Create a simple Python server if serve.py doesn't exist
if [ ! -f "serve.py" ]; then
    python3 -m http.server 8080 --bind 0.0.0.0
else
    python3 serve.py
fi