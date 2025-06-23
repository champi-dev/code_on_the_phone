# Quantum Terminal - Cross-Platform Makefile

CC = clang
CFLAGS = -Wall -Wextra -O3 -march=native -ffast-math
INCLUDES = -Isrc -Iinclude -Ilib

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Source files
CORE_SRCS = src/terminal.c src/pty.c src/renderer.c src/quantum.c src/input.c
PLATFORM_SRCS = 

# macOS configuration
ifeq ($(UNAME_S),Darwin)
	PLATFORM = MACOS
	CFLAGS += -DMACOS -framework Cocoa -framework Metal -framework MetalKit -framework IOKit
	PLATFORM_SRCS += src/platform/macos.m src/platform/metal_renderer.m
	LDFLAGS = -framework OpenGL -framework GLUT
endif

# Linux configuration
ifeq ($(UNAME_S),Linux)
	PLATFORM = LINUX
	CFLAGS += -DLINUX
	PLATFORM_SRCS += src/platform/linux.c src/platform/gl_renderer.c
	LDFLAGS = -lGL -lGLEW -lglfw -lm -lpthread
endif

# Common directories
OBJDIR = obj
BINDIR = build

# Targets
all: dirs $(BINDIR)/quantum-terminal

dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)
	@mkdir -p $(OBJDIR)/platform

# macOS app bundle
macos: dirs
	@echo "Building Quantum Terminal for macOS..."
	@mkdir -p build/QuantumTerminal.app/Contents/{MacOS,Resources}
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE_SRCS) src/main.c src/platform/macos.m src/platform/metal_renderer.m \
		-o build/QuantumTerminal.app/Contents/MacOS/QuantumTerminal \
		-framework Cocoa -framework Metal -framework MetalKit -framework IOKit -framework CoreVideo -framework QuartzCore
	@cp resources/Info.plist build/QuantumTerminal.app/Contents/
	@cp resources/icon.icns build/QuantumTerminal.app/Contents/Resources/
	@echo "✅ macOS build complete!"

# iOS build (requires Xcode)
ios:
	@echo "Building Quantum Terminal for iOS..."
	@mkdir -p build/ios
	xcrun -sdk iphoneos clang $(CFLAGS) -DIOS -arch arm64 \
		$(CORE_SRCS) src/platform/ios.m src/platform/metal_renderer.m \
		-framework UIKit -framework Metal -framework MetalKit \
		-o build/ios/QuantumTerminal
	@echo "✅ iOS build ready for Xcode!"

# Android build (requires NDK)
android:
	@echo "Building Quantum Terminal for Android..."
	@mkdir -p build/android/jni build/android/libs
	@cp android/jni/* build/android/jni/
	ndk-build NDK_PROJECT_PATH=build/android APP_BUILD_SCRIPT=Android.mk
	@echo "✅ Android build complete!"

# WebAssembly build
wasm: dirs
	@echo "Building Quantum Terminal for Web..."
	@mkdir -p build/web
	emcc $(CORE_SRCS) src/platform/web.c src/platform/webgl_renderer.c \
		-O3 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 \
		-s EXPORTED_FUNCTIONS='["_main", "_terminal_input", "_terminal_resize"]' \
		-s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
		--preload-file resources \
		-o build/web/quantum-terminal.js
	@cp resources/index.html build/web/
	@echo "✅ WebAssembly build complete!"

# Standard build for current platform
$(BINDIR)/quantum-terminal: $(CORE_SRCS) $(PLATFORM_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Windows cross-compilation
windows:
	@echo "Building Quantum Terminal for Windows..."
	@mkdir -p build/windows
	x86_64-w64-mingw32-gcc -o build/windows/quantum-terminal.exe \
		src/platform/windows.c src/quantum.c src/renderer.c \
		-lopengl32 -lglu32 -luser32 -lgdi32 -static
	@echo "✅ Windows build complete!"

# Linux with GTK
linux-gtk:
	@echo "Building Quantum Terminal for Linux (GTK)..."
	@mkdir -p build/linux
	gcc -o build/linux/quantum-terminal-gtk \
		src/platform/linux_gtk.c src/quantum.c src/renderer.c \
		`pkg-config --cflags --libs gtk+-3.0 vte-2.91` \
		-lGL -lGLU -lm -O3
	@echo "✅ Linux GTK build complete!"

# Universal AppImage
appimage: linux-gtk
	@echo "Creating AppImage..."
	@mkdir -p build/appimage/usr/bin
	@cp build/linux/quantum-terminal-gtk build/appimage/usr/bin/
	@cp resources/quantum-terminal.desktop build/appimage/
	@cp resources/icon.png build/appimage/
	appimagetool build/appimage build/QuantumTerminal.AppImage
	@echo "✅ AppImage created!"

# Snap package
snap:
	@echo "Building Snap package..."
	snapcraft
	@echo "✅ Snap package built!"

# Cocoa terminal (your current implementation)
cocoa:
	@echo "Building Cocoa Terminal..."
	clang -framework Cocoa -framework OpenGL -framework Foundation \
		-o cocoa_terminal src/cocoa_terminal.m \
		-fobjc-arc -DGL_SILENCE_DEPRECATION \
		-Wall -O2
	@echo "✅ Cocoa Terminal built!"

.PHONY: all dirs macos ios android wasm clean windows linux-gtk appimage snap cocoa