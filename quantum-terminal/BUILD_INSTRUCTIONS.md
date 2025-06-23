# Quantum Terminal - Build Instructions

## macOS

### Prerequisites
- Xcode 12+ with Command Line Tools
- macOS 10.15+

### Build
```bash
cd quantum-terminal
make macos
```

### Run
```bash
./build/QuantumTerminal.app/Contents/MacOS/QuantumTerminal
```

Or double-click the app in Finder:
```
open build/QuantumTerminal.app
```

## iOS

### Prerequisites
- Xcode 12+
- iOS Developer Account (for device deployment)

### Build
```bash
make ios
```

Then:
1. Open `build/ios/QuantumTerminal.xcodeproj` in Xcode
2. Select your development team
3. Build and run on device/simulator

## Android

### Prerequisites
- Android NDK r21+
- Android SDK
- Java 8+

### Setup
```bash
export ANDROID_NDK_HOME=/path/to/android-ndk
export PATH=$ANDROID_NDK_HOME:$PATH
```

### Build
```bash
make android
```

### Install
```bash
adb install -r build/android/quantum-terminal.apk
```

## Web (WebAssembly)

### Prerequisites
- Emscripten SDK
- Python 3 (for local server)

### Setup
```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### Build
```bash
make wasm
```

### Run
```bash
cd build/web
python3 -m http.server 8000
# Open http://localhost:8000 in browser
```

## Linux

### Prerequisites
- GCC or Clang
- OpenGL development libraries
- GLFW3

### Install Dependencies
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libglfw3-dev libglew-dev

# Fedora
sudo dnf install gcc glfw-devel glew-devel

# Arch
sudo pacman -S base-devel glfw glew
```

### Build
```bash
make
./build/quantum-terminal
```

## Features by Platform

| Feature | macOS | iOS | Android | Web | Linux |
|---------|-------|-----|---------|-----|-------|
| Terminal Emulation | ✅ | ✅ | ✅ | ✅ | ✅ |
| 3D Particles | ✅ | ✅ | ✅ | ✅ | ✅ |
| Metal Rendering | ✅ | ✅ | ❌ | ❌ | ❌ |
| OpenGL | ✅ | ❌ | ✅ | ✅ | ✅ |
| Touch Input | ❌ | ✅ | ✅ | ✅ | ❌ |
| Hardware Keyboard | ✅ | ✅ | ✅ | ✅ | ✅ |
| Copy/Paste | ✅ | ✅ | ✅ | ⚠️ | ✅ |

## Performance Tips

- **macOS**: Use Metal renderer for best performance
- **iOS**: Reduce particle count on older devices
- **Android**: Enable hardware acceleration
- **Web**: Use Chrome/Firefox for best WebGL performance
- **Linux**: Ensure GPU drivers are up to date

## Troubleshooting

### macOS: "App is damaged"
```bash
xattr -cr build/QuantumTerminal.app
```

### Linux: Missing PTY
```bash
sudo apt-get install libutil-freebsd-dev
```

### Android: NDK not found
Set `ANDROID_NDK_HOME` environment variable

### Web: SharedArrayBuffer error
Serve with proper CORS headers for threading support