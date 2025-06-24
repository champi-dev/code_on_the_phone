# Quantum Terminal - Flutter & Go Edition

A beautiful, cross-platform terminal emulator with stunning 3D quantum particle animations, built with Flutter and Go.

## Features

- 🎨 **Stunning Animations**: 10+ quantum particle effects triggered by popular commands
- 🚀 **Flutter Frontend**: Beautiful, responsive UI with smooth 60fps animations
- 🔧 **Go Backend**: High-performance PTY management and SSH connections
- 🌊 **Digital Ocean Integration**: Connect directly to your droplets
- 🔌 **WebSocket Communication**: Real-time bidirectional data streaming
- 📱 **Cross-Platform**: Works on iOS, Android, Web, Desktop (Windows, macOS, Linux)

## Architecture

```
flutter-quantum-terminal/
├── frontend/           # Flutter app
│   ├── lib/
│   │   ├── main.dart
│   │   ├── screens/
│   │   ├── widgets/
│   │   ├── animations/
│   │   └── services/
│   └── pubspec.yaml
│
└── backend/           # Go server
    ├── main.go
    ├── terminal/      # PTY management
    ├── websocket/     # WebSocket server
    └── ssh/           # SSH client for DO droplet
```

## Animations

Same spectacular effects as before, but now with Flutter's powerful animation system:

- 🌧️ **Matrix Rain** (`ls`) - Cascading green characters
- 🌀 **Wormhole Portal** (`cd`) - Swirling vortex effect
- 💥 **Quantum Explosion** (`rm -rf`) - Particle burst
- 🧬 **DNA Helix** (`git`) - Double helix animation
- 🔀 **Glitch Effect** (`sudo`) - RGB channel separation
- And many more!

## Quick Start

### Backend (Go)
```bash
cd backend
go mod init quantum-terminal-backend
go get github.com/gorilla/websocket
go get github.com/creack/pty
go get golang.org/x/crypto/ssh
go run main.go
```

### Frontend (Flutter)
```bash
cd frontend
flutter create . --project-name quantum_terminal
flutter pub add web_socket_channel
flutter pub add provider
flutter pub add animated_background
flutter run
```

## Digital Ocean Connection

Configure your droplet connection in `backend/config.json`:
```json
{
  "droplet": {
    "host": "your-droplet-ip",
    "port": 22,
    "username": "root",
    "keyPath": "~/.ssh/id_rsa"
  }
}
```

## Development

- Frontend uses Flutter's animation controllers and custom painters
- Backend uses goroutines for concurrent terminal sessions
- WebSocket protocol for real-time terminal updates
- SSH tunneling for secure droplet connections