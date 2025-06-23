# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Quick Launch (macOS)
```bash
./run.sh  # Builds if needed and launches the app
```

### Platform-Specific Builds
```bash
# macOS (Metal rendering)
make macos
./build/QuantumTerminal.app/Contents/MacOS/QuantumTerminal

# iOS
make ios
# Then open build/ios/QuantumTerminal.xcodeproj in Xcode

# Android
make android
adb install -r build/android/quantum-terminal.apk

# Web (WebAssembly)
make wasm
cd build/web && python3 -m http.server 8000

# Linux
make
./build/quantum-terminal
```

### Development Commands
```bash
make clean  # Clean build artifacts
make dirs   # Create necessary directories
```

## Architecture Overview

Quantum Terminal is a high-performance, cross-platform terminal emulator written in pure C with 3D quantum particle animations. The codebase follows a modular architecture with platform abstraction.

### Core Components
- **Terminal Emulation** (`src/terminal.c`): PTY management and terminal state
- **Renderer** (`src/renderer.c`): Core rendering logic with platform backends
- **Quantum System** (`src/quantum.c`): 3D particle physics and animations
- **Input Handling** (`src/input.c`): Keyboard and touch input processing
- **Platform Layer** (`src/platform/`): Platform-specific implementations

### Platform Abstraction Strategy
- Common core in pure C (`src/terminal.c`, `src/pty.c`, `src/renderer.c`, `src/quantum.c`)
- Platform-specific code isolated in `src/platform/`:
  - `macos.m` & `metal_renderer.m`: macOS/iOS Metal implementation
  - `linux.c` & `gl_renderer.c`: Linux OpenGL implementation
  - `web.c` & `webgl_renderer.c`: WebAssembly/WebGL implementation
  - `ios.m`: iOS-specific UI integration
  - Android implementation via NDK

### Rendering Architecture
- **Metal** (macOS/iOS): Native GPU rendering with Metal shaders
- **OpenGL/WebGL** (Linux/Web/Android): Cross-platform GPU rendering
- Shaders in `src/shaders/`: `particle.metal` and `terminal.metal`
- Zero-copy render loop with hardware acceleration

### Performance Features
- SIMD-optimized particle physics
- Lock-free ring buffers for I/O
- Zero-allocation render loop
- Hardware-accelerated rendering (Metal/OpenGL)
- Native architecture targeting with `-O3 -march=native`

### Build System
The Makefile handles cross-platform builds with platform detection:
- Compiler: Clang by default
- Optimization: `-O3 -march=native -ffast-math`
- Platform-specific frameworks and libraries linked automatically

## C Port Subproject

The repository contains a C port of a cloud terminal server in the `c-port/` directory:

### C Port Build Commands
```bash
cd c-port

# Debug build
make MODE=debug

# Release build (optimized)
make

# Run tests
make test

# Format code
make format

# Lint
make lint
```

### C Port Architecture
High-performance HTTP/WebSocket server with:
- O(1) or O(log n) complexity for all operations
- Zero-copy I/O with splice/sendfile
- Lock-free data structures
- Memory pool allocation
- Event-driven architecture (epoll/kqueue)

Key components implemented:
- Memory pool allocator
- Lock-free ring buffer
- Hash table with O(1) lookups
- Red-black tree for session expiry
- High-performance event loop

## Key Notes

- No test infrastructure exists yet in the main quantum-terminal project
- The C port has test infrastructure with `make test`
- Both projects use aggressive optimizations for performance
- Platform-specific code is well-isolated for easy porting