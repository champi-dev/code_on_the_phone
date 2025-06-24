# Quantum Terminal Setup Guide

## Prerequisites

1. **Go** (1.21 or later)
   ```bash
   # Check version
   go version
   ```

2. **Flutter** (3.0 or later)
   ```bash
   # Check version
   flutter --version
   ```

3. **Digital Ocean Droplet** (optional)
   - SSH access configured
   - Your SSH key at `~/.ssh/id_rsa`

## Quick Start

1. **Configure your droplet** (optional):
   ```bash
   cd backend
   cp config.json.example config.json
   # Edit config.json with your droplet details
   ```

2. **Run everything**:
   ```bash
   ./run.sh
   ```

   Or run separately:
   ```bash
   # Terminal 1 - Backend
   cd backend
   go run main.go

   # Terminal 2 - Frontend
   cd frontend
   flutter run -d chrome
   ```

## Features Demo

Once running, try these commands in the terminal to see animations:

- `ls` → Matrix rain effect 🌧️
- `cd /home` → Wormhole portal 🌀
- `git status` → DNA double helix 🧬
- `sudo apt update` → Glitch effect 🔀
- `make` → Particle fountain ⛲
- `python` → Neural network 🧠
- `vim file.txt` → Cosmic rays ⚡
- `history` → Time warp ⏰
- `ssh user@host` → Quantum tunnel 🚇

## Connecting to Digital Ocean

1. Click "Connect to Droplet" button
2. Enter your droplet's IP address
3. Make sure your SSH key is configured in `backend/config.json`

## Development

### Backend Structure
```
backend/
├── main.go           # WebSocket server & PTY management
├── terminal/         # Terminal emulation
├── websocket/        # WebSocket handlers
└── ssh/             # SSH client for droplets
```

### Frontend Structure
```
frontend/
├── lib/
│   ├── main.dart
│   ├── screens/     # UI screens
│   ├── widgets/     # Reusable widgets
│   ├── animations/  # Particle system
│   └── services/    # Business logic
```

## Customization

### Add New Animations

1. Add animation type to `backend/main.go`:
   ```go
   AnimationNewEffect AnimationType = "new_effect"
   ```

2. Add trigger in command map:
   ```go
   "newcmd": AnimationNewEffect,
   ```

3. Implement in `frontend/lib/widgets/quantum_animation_overlay.dart`:
   ```dart
   void _createNewEffect(AnimationTrigger trigger) {
     // Your particle creation code
   }
   ```

### Change Colors/Theme

Edit `frontend/lib/main.dart` theme configuration.

## Troubleshooting

- **Backend won't start**: Check if port 8080 is available
- **Frontend connection fails**: Ensure backend is running first
- **Droplet connection fails**: Verify SSH key permissions (600)
- **Animations lag**: Reduce particle count in animations

## Production Deployment

1. **Backend**: Deploy to your server or cloud function
2. **Frontend**: Build and deploy:
   ```bash
   flutter build web
   # Deploy build/web/ to your hosting
   ```

Enjoy your quantum terminal! 🚀✨