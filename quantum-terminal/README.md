# Quantum Terminal - High-Performance Cross-Platform Terminal

A blazing-fast terminal emulator written in pure C with stunning 3D quantum particle animations, supporting macOS, iOS, Android, and Web.

## Features

- **Pure C Implementation** - Maximum performance, minimal overhead
- **3D Quantum Animations** - Smooth particle effects using OpenGL/Metal
- **Cross-Platform** - Single codebase for all platforms
- **Hardware Accelerated** - Native GPU rendering
- **Zero-Copy Architecture** - Optimal memory usage
- **60+ FPS** - Butter-smooth animations

## Platforms

- **macOS** - Native app with Metal rendering
- **iOS** - Full terminal on iPhone/iPad
- **Android** - NDK-based native performance
- **Web** - WebAssembly + WebGL
- **Linux/Windows** - OpenGL rendering

## Quick Start

### macOS
```bash
make macos
./build/QuantumTerminal.app/Contents/MacOS/QuantumTerminal
```

### iOS
```bash
make ios
# Open in Xcode and deploy
```

### Android
```bash
make android
# Install APK from build/android/
```

### Web
```bash
make wasm
python3 -m http.server --directory build/web
```

## Architecture

- Terminal emulation using native PTY
- OpenGL ES 3.0 / Metal for rendering
- Lock-free ring buffers for I/O
- SIMD-optimized particle physics
- Zero-allocation render loop