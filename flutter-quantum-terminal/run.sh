#!/bin/bash

# Quantum Terminal Runner Script

echo "🚀 Starting Quantum Terminal..."
echo "================================"

# Check if running backend or frontend
if [ "$1" = "backend" ] || [ -z "$1" ]; then
    echo "📡 Starting Go backend..."
    cd backend
    
    # Install dependencies if needed
    if [ ! -d "vendor" ]; then
        echo "📦 Installing Go dependencies..."
        go mod download
    fi
    
    # Run backend
    go run main.go &
    BACKEND_PID=$!
    echo "✅ Backend started (PID: $BACKEND_PID)"
fi

if [ "$1" = "frontend" ] || [ -z "$1" ]; then
    echo "🎨 Starting Flutter frontend..."
    cd frontend
    
    # Install dependencies if needed
    if [ ! -d ".dart_tool" ]; then
        echo "📦 Installing Flutter dependencies..."
        flutter pub get
    fi
    
    # Run frontend
    echo "🌐 Opening in browser..."
    flutter run -d chrome --web-port=3000 &
    FRONTEND_PID=$!
    echo "✅ Frontend started (PID: $FRONTEND_PID)"
fi

echo ""
echo "================================"
echo "✨ Quantum Terminal is running!"
echo ""
echo "🌐 Frontend: http://localhost:3000"
echo "📡 Backend: http://localhost:8080"
echo ""
echo "Press Ctrl+C to stop"
echo "================================"

# Wait for Ctrl+C
trap "echo '🛑 Stopping...'; kill $BACKEND_PID $FRONTEND_PID 2>/dev/null; exit" INT
wait