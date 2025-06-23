#!/bin/bash
# Run the Cocoa/OpenGL terminal

cd "$(dirname "$0")"

echo "🚀 Building and launching Quantum Terminal (Cocoa version)..."
echo "✨ This version uses native Cocoa with OpenGL"
echo ""

# Compile with ARC and suppress deprecation warnings
clang -framework Cocoa -framework OpenGL -framework Foundation \
      -o cocoa_terminal src/cocoa_terminal.m \
      -fobjc-arc -DGL_SILENCE_DEPRECATION \
      -Wall -O2

if [ $? -eq 0 ]; then
    echo "Build successful! Launching..."
    ./cocoa_terminal
else
    echo "Build failed!"
    exit 1
fi