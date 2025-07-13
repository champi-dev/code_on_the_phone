# Local Testing Guide

## Quick Start

```bash
# Navigate to the project
cd rust-web-terminal

# Run the local test script
./test-locally.sh
```

Then open your browser to: http://localhost:8080

## Manual Testing Steps

### 1. Install Prerequisites

```bash
# Install Rust (if not installed)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Add WebAssembly target
rustup target add wasm32-unknown-unknown

# Install wasm-pack
cargo install wasm-pack
# OR
npm install -g wasm-pack
```

### 2. Build the Project

```bash
# Development build (faster, with debug info)
wasm-pack build --target web --out-dir pkg --dev

# Production build (optimized)
wasm-pack build --target web --out-dir pkg --release
```

### 3. Start Local Server

The server binds to 0.0.0.0 by default, allowing access from any network interface.

Option A: Using Python (binds to 0.0.0.0)
```bash
python3 -m http.server 8080 --bind 0.0.0.0
```

Option B: Using Node.js (binds to 0.0.0.0)
```bash
npx serve -p 8080 -l 0.0.0.0
```

Option C: Using the included server (recommended)
```bash
python3 serve.py
```

Option D: Using Rust's basic-http-server
```bash
cargo install basic-http-server
basic-http-server -a 0.0.0.0:8080
```

### 4. Test in Browser

Access the terminal from:
- **Local machine**: http://localhost:8080
- **Same network**: http://<your-ip>:8080
- **Mobile device**: http://<your-ip>:8080

To find your IP address:
```bash
# Linux/Mac
hostname -I | awk '{print $1}'

# or
ip addr show | grep "inet " | grep -v 127.0.0.1

# Windows
ipconfig | findstr /i "IPv4"
```

Testing steps:
1. Open the URL in your browser
2. The connection dialog will appear with pre-filled values:
   - Host: 142.93.249.123
   - Password: cloudterm123
3. Click "Connect"
4. You should see the terminal connect to your droplet

## Running Tests

### Unit Tests
```bash
# Run Rust unit tests
wasm-pack test --chrome --headless

# Or with Firefox
wasm-pack test --firefox --headless
```

### E2E Tests
```bash
# Install dependencies
npm install

# Run E2E tests
npm test

# Run with visible browser
npm run test:headed

# Debug mode
npm run test:debug
```

### All Tests
```bash
# Run complete test suite
./run-tests.sh
```

## Troubleshooting

### Build Errors

1. **"error: can't find crate"**
   ```bash
   # Update dependencies
   cargo update
   ```

2. **"wasm-pack: command not found"**
   ```bash
   # Install via npm
   npm install -g wasm-pack
   ```

3. **"target wasm32-unknown-unknown not found"**
   ```bash
   rustup target add wasm32-unknown-unknown
   ```

### Connection Issues

1. **"WebSocket connection failed"**
   - Check if your droplet (142.93.249.123) is accessible
   - Verify ttyd is running on port 7681
   - Check browser console for errors

2. **"CORS error"**
   - Make sure you're accessing via http://localhost:8080
   - Not file:// protocol

3. **"WASM module failed to load"**
   - Clear browser cache
   - Check browser supports WebAssembly
   - Verify pkg/ directory exists with .wasm file

### Browser Compatibility

Tested and working on:
- ✅ Chrome 90+
- ✅ Firefox 89+
- ✅ Safari 14.1+
- ✅ Edge 90+

## Development Tips

### Watch Mode
```bash
# Install cargo-watch
cargo install cargo-watch

# Auto-rebuild on changes
cargo watch -s "wasm-pack build --target web --out-dir pkg --dev"
```

### Browser DevTools
1. Open Chrome DevTools (F12)
2. Go to Sources tab
3. Enable WebAssembly debugging
4. Set breakpoints in Rust code

### Performance Profiling
1. Open Chrome DevTools
2. Go to Performance tab
3. Start recording
4. Interact with terminal
5. Stop and analyze

## Testing Without Real Droplet

To test without connecting to the actual droplet, you can:

1. **Mock Server**: Create a local WebSocket echo server
   ```bash
   # Install ws
   npm install -g ws
   
   # Create echo server
   wscat -l 7681
   ```

2. **Update Host**: Change the host in index.html to `localhost`

3. **Use Docker**: Run ttyd locally
   ```bash
   docker run -p 7681:7681 -it tsl0922/ttyd bash
   ```

## Directory Structure After Build

```
rust-web-terminal/
├── pkg/                    # Built WASM files
│   ├── rust_web_terminal_bg.wasm
│   ├── rust_web_terminal.js
│   └── package.json
├── target/                 # Rust build artifacts
├── node_modules/          # JS dependencies (after npm install)
├── screenshots/           # E2E test screenshots
└── test-results/         # Test reports
```