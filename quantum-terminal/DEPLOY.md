# Quantum Terminal - Cross-Platform Deployment Guide

## Prerequisites

### All Platforms
- Git
- Make
- Clang or GCC

### Platform-Specific Requirements

**macOS**
- Xcode Command Line Tools
- macOS 10.14+

**iOS**
- Xcode 12+
- Apple Developer Account (for device deployment)
- iOS 13+

**Android**
- Android Studio or Android SDK
- Android NDK r21+
- Java JDK 11+

**Windows**
- MinGW-w64 or Visual Studio 2019+
- Windows SDK

**Linux**
- OpenGL development libraries
- GTK3 or Qt5 (optional for better integration)

**Web**
- Emscripten SDK
- Python 3 (for local testing)

## Build Instructions

### macOS

```bash
# Build native macOS app
make macos

# Run the app
open build/QuantumTerminal.app

# Create DMG for distribution
hdiutil create -volname "Quantum Terminal" -srcfolder build/QuantumTerminal.app -ov -format UDZO quantum-terminal-macos.dmg
```

### iOS

```bash
# Build iOS framework
make ios

# Open in Xcode
open ios/QuantumTerminal.xcodeproj

# Or deploy directly to device (requires signing)
ios-deploy --bundle build/ios/QuantumTerminal.app
```

### Android

```bash
# Ensure Android SDK/NDK paths are set
export ANDROID_SDK_ROOT=/path/to/android-sdk
export ANDROID_NDK_ROOT=/path/to/android-ndk

# Build APK
make android

# Install on device
adb install -r build/android/quantum-terminal.apk

# Or build AAB for Play Store
cd android
./gradlew bundleRelease
```

### Windows

Since our current implementation uses Cocoa/OpenGL, we need a Windows port:

```bash
# Using MinGW-w64
x86_64-w64-mingw32-gcc -o quantum-terminal.exe \
  src/terminal.c src/renderer.c src/quantum.c \
  src/platform/windows.c src/platform/d3d_renderer.c \
  -lopengl32 -lgdi32 -luser32

# Or use provided Windows makefile
make -f Makefile.windows
```

### Linux

```bash
# Install dependencies
sudo apt-get install libgl1-mesa-dev libglew-dev libglfw3-dev

# Build
make

# Run
./build/quantum-terminal

# Create AppImage
make appimage

# Create Snap package
snapcraft
```

### Web (WebAssembly)

```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Build WASM version
make wasm

# Test locally
cd build/web
python3 -m http.server 8000
# Open http://localhost:8000
```

## Platform-Specific Features

### macOS
- Metal rendering for best performance
- Touch Bar support
- Native menu integration

### iOS
- Touch-optimized controls
- External keyboard support
- Split View/Slide Over

### Android
- Virtual keyboard with special keys
- Gesture navigation
- Samsung DeX support

### Windows
- DirectX/OpenGL rendering
- Windows Terminal integration
- WSL support

### Linux
- Multiple rendering backends (OpenGL, Vulkan)
- Wayland/X11 support
- Package manager integration

## Distribution

### App Store (iOS/macOS)
1. Create app in App Store Connect
2. Generate provisioning profiles
3. Archive in Xcode
4. Upload with Xcode or Transporter

### Google Play Store
1. Generate signed AAB
2. Create app listing
3. Upload to Play Console
4. Submit for review

### Microsoft Store
1. Package as MSIX
2. Create app in Partner Center
3. Submit for certification

### Linux Repositories
- **Ubuntu**: Create PPA
- **Fedora**: Submit to COPR
- **Arch**: Submit to AUR
- **Universal**: Flatpak/Snap

## Code Signing

### macOS/iOS
```bash
codesign --deep --force --verify --verbose \
  --sign "Developer ID Application: Your Name" \
  build/QuantumTerminal.app
```

### Android
```bash
keytool -genkey -v -keystore quantum.keystore \
  -alias quantum -keyalg RSA -keysize 2048 -validity 10000

jarsigner -verbose -sigalg SHA256withRSA -digestalg SHA-256 \
  -keystore quantum.keystore build/android/quantum-terminal.apk quantum
```

### Windows
```powershell
signtool sign /f quantum.pfx /p password /tr http://timestamp.digicert.com quantum-terminal.exe
```

## Continuous Integration

Create `.github/workflows/build.yml`:

```yaml
name: Build All Platforms

on: [push, pull_request]

jobs:
  build-macos:
    runs-on: macos-latest
    steps:
    - uses: actions/checkout@v2
    - run: make macos
    - uses: actions/upload-artifact@v2
      with:
        name: macos-build
        path: build/QuantumTerminal.app

  build-linux:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - run: sudo apt-get install -y libgl1-mesa-dev libglew-dev libglfw3-dev
    - run: make
    - uses: actions/upload-artifact@v2
      with:
        name: linux-build
        path: build/quantum-terminal

  build-android:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - uses: android-actions/setup-android@v2
    - run: make android
    - uses: actions/upload-artifact@v2
      with:
        name: android-build
        path: build/android/*.apk
```

## Testing

- **macOS/iOS**: Use Xcode's testing framework
- **Android**: Use Espresso for UI tests
- **All platforms**: Use the test suite in `tests/`

## Performance Tips

1. Enable release optimizations
2. Use platform-specific rendering (Metal on Apple, DirectX on Windows)
3. Profile with platform tools (Instruments, Android Studio Profiler, etc.)
4. Consider using platform-specific terminal APIs where available

## Troubleshooting

- **macOS**: If app won't open, check Gatekeeper settings
- **iOS**: Ensure provisioning profiles are valid
- **Android**: Check minimum SDK version (21+)
- **Windows**: Install Visual C++ Redistributables
- **Linux**: Check OpenGL version (3.3+ required)