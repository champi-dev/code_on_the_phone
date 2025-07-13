# Rust Web Terminal

A high-performance web-based terminal client written in Rust and compiled to WebAssembly. This terminal connects to remote servers via WebSocket and provides a full terminal experience in the browser.

## Features

- 🚀 **High Performance**: Written in Rust and compiled to WASM for near-native performance
- 🔒 **Secure**: Encrypted WebSocket connections with authentication
- 📱 **Responsive**: Works on desktop and mobile devices
- 🎨 **Modern UI**: Clean, GitHub-inspired dark theme
- 📸 **Fully Tested**: Comprehensive unit and E2E tests with screenshots
- ⚡ **Real-time**: Low-latency terminal interaction

## Architecture

```
┌─────────────────┐
│   Browser UI    │
│  (HTML/CSS/JS)  │
└────────┬────────┘
         │
┌────────▼────────┐
│   Rust WASM     │
│   WebTerminal   │
└────────┬────────┘
         │
┌────────▼────────┐
│   WebSocket     │
│   Connection    │
└────────┬────────┘
         │
┌────────▼────────┐
│  Remote Droplet │
│ (142.93.249.123)│
└─────────────────┘
```

## Quick Start

1. **Build the project**:
   ```bash
   ./build.sh
   ```

2. **Run the development server**:
   ```bash
   ./serve.py
   ```

3. **Open in browser**:
   Navigate to `http://localhost:8080`

4. **Connect to droplet**:
   - Host: `142.93.249.123`
   - Password: `cloudterm123`

## Testing

### Run all tests with screenshots:
```bash
./run-tests.sh
```

This will:
- Run Rust unit tests
- Run E2E tests with Playwright
- Capture screenshots at each stage
- Generate a test report

### Test Results

After running tests, you'll find:
- **Screenshots**: `screenshots/` directory
- **Test Report**: `http://localhost:9323`
- **JUnit XML**: `test-results/junit.xml`
- **JSON Results**: `test-results/results.json`

## Project Structure

```
rust-web-terminal/
├── src/
│   ├── lib.rs          # Main library entry point
│   ├── terminal.rs     # Terminal UI integration
│   ├── websocket.rs    # WebSocket client
│   ├── auth.rs         # Authentication logic
│   └── utils.rs        # Utility functions
├── tests/
│   ├── e2e/            # End-to-end tests
│   └── unit_tests.rs   # Rust unit tests
├── index.html          # Web interface
├── build.sh            # Build script
├── serve.py            # Dev server
└── run-tests.sh        # Test runner
```

## Key Components

### WebTerminal (Rust)
The main struct that manages the terminal lifecycle:
- Creates and manages xterm.js instance
- Handles WebSocket connection
- Manages authentication
- Processes terminal I/O

### WebSocket Protocol
Uses ttyd binary protocol:
- Message type 0: Terminal input/output
- Message type 1: Terminal resize
- Binary frames for efficiency

### Terminal UI
- Uses xterm.js for terminal rendering
- FitAddon for responsive sizing
- WebLinksAddon for clickable URLs

## Development

### Prerequisites
- Rust (with wasm32-unknown-unknown target)
- wasm-pack
- Node.js (for testing)
- Python 3 (for dev server)

### Building
```bash
# Install Rust target
rustup target add wasm32-unknown-unknown

# Install wasm-pack
curl https://rustwasm.github.io/wasm-pack/installer/init.sh -sSf | sh

# Build the project
./build.sh
```

### Testing
```bash
# Unit tests only
wasm-pack test --chrome --headless

# E2E tests only
npm test

# All tests with coverage
./run-tests.sh
```

## Performance

The terminal is optimized for:
- **Low latency**: Direct WebSocket connection
- **High throughput**: Binary message format
- **Smooth rendering**: Efficient DOM updates
- **Memory efficiency**: Rust's zero-cost abstractions

## Security

- Password-protected access
- Encrypted WebSocket connections (WSS in production)
- No sensitive data stored client-side
- Secure authentication flow

## Browser Support

- ✅ Chrome/Edge (latest)
- ✅ Firefox (latest)
- ✅ Safari (latest)
- ✅ Mobile browsers

## License

MIT License - See LICENSE file for details