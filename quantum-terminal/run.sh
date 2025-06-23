#!/bin/bash
# Launch Quantum Terminal

cd "$(dirname "$0")"

if [ ! -f "build/QuantumTerminal.app/Contents/MacOS/QuantumTerminal" ]; then
    echo "Building Quantum Terminal..."
    make macos
fi

echo "Launching Quantum Terminal..."
open build/QuantumTerminal.app