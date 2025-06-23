# Terminal Projects Suite - Cloud & Native Terminal Solutions

A collection of high-performance terminal implementations including cloud-based remote terminals and native terminal emulators with 3D quantum animations.

## 📋 Project Components

### 1. **Cloud Terminal** (Remote Terminal Access)
Web-based terminal that connects to remote VPS servers via WebSocket proxy. Access your development environment from anywhere.

### 2. **Quantum Terminal** (Native Terminal Emulator)
High-performance native terminal emulator written in pure C with stunning 3D particle animations. Runs locally on macOS, iOS, Android, Linux, and Web.

### 3. **C Port** (High-Performance Web Server)
Optimized HTTP/WebSocket server implementation with O(1) performance characteristics.

## 🚀 Quick Start Options

### Option 1: Instant Cloud Terminals (No Setup Required!)

#### **Google Cloud Shell** (Recommended - FREE)
- **URL**: https://shell.cloud.google.com
- **Setup Time**: 0 seconds
- **Cost**: FREE (50 hours/week)
- **Features**: Python, Node.js, Docker, 5GB storage
- Just click the link and start coding!

#### **GitHub Codespaces**
- Go to any GitHub repo and press `.` (period)
- Or visit: https://github.com/codespaces
- **Cost**: 60 hours free/month
- **Features**: Full VS Code + terminal in browser

#### **Replit**
- **URL**: https://replit.com
- **Cost**: Free tier available
- **Features**: Instant terminal, always-on URLs

### Option 2: Native Terminal (Quantum Terminal)

Run a high-performance terminal emulator locally with 3D animations:

#### **macOS/Linux**
```bash
cd quantum-terminal
./run.sh  # Builds and launches automatically
```

#### **iOS/Android**
```bash
cd quantum-terminal
make ios    # or: make android
# Follow platform-specific deployment instructions
```

### Option 3: Self-Hosted Cloud Terminal (VPS)

Deploy your own cloud terminal server:

#### **DigitalOcean** (Fastest setup)
```bash
# 1. Create account: https://digitalocean.com ($200 free credit)
# 2. Create Ubuntu 24.04 droplet ($12/month)
# 3. SSH in and run:
curl -sSL https://raw.githubusercontent.com/your-repo/quick-setup.sh | bash
```

#### **Oracle Cloud** (FREE Forever!)
- **Specs**: 4 vCPU, 24GB RAM (ARM)
- **Cost**: FREE
- **URL**: https://oracle.com/cloud/free/

#### **Vultr** (Cheapest)
- **Cost**: $6/month (2GB RAM)
- **URL**: https://vultr.com

### Option 4: Deploy Frontend with Render/Vercel

Use our pre-built frontend with your VPS:

#### **Render Deployment**
1. Set up any VPS with our script
2. Deploy `render-terminal/` to Render.com
3. Access at `https://your-app.onrender.com`

#### **Vercel Deployment**
1. Set up any VPS with our script
2. Deploy `vercel-terminal/` to Vercel
3. Access at `https://your-app.vercel.app`

## 📦 Features by Component

### Cloud Terminal (Remote)
- **Access Method**: Web browser via WebSocket proxy
- **Backend**: ttyd terminal server on VPS
- **Authentication**: Password-protected with bcrypt
- **Session Management**: Persistent sessions with tmux
- **Mobile Support**: Responsive UI with touch controls
- **Deployment**: Render, Vercel, or any web host

### Quantum Terminal (Native)
- **3D Animations**: OpenGL/Metal quantum particle effects
- **Performance**: 60+ FPS with hardware acceleration
- **Platforms**: macOS, iOS, Android, Linux, Web (WASM)
- **Architecture**: Pure C with zero-copy I/O
- **Terminal Engine**: Native PTY with full VT100 support
- **Input**: Keyboard and touch optimized

### Development Tools (VPS Setup)
- **Languages**: Python 3, Node.js, Go, Rust, Java
- **Package Managers**: pip, npm, yarn, pnpm, cargo
- **Version Control**: git, gh (GitHub CLI)
- **Math/Science**: NumPy, SciPy, SymPy, SageMath, Octave
- **Machine Learning**: PyTorch, TensorFlow, scikit-learn
- **Data Science**: Pandas, Matplotlib, Jupyter
- **Shell**: Zsh with Oh My Zsh + Starship prompt
- **Tools**: ripgrep, fzf, bat, exa, htop

## 🎯 Quick Decision Guide

**Want a local terminal with 3D animations?**
→ Use Quantum Terminal (runs natively)

**Want it NOW with zero setup?**
→ Use Google Cloud Shell

**Need persistent cloud development environment?**
→ Deploy Cloud Terminal on VPS

**Want it completely FREE?**
→ Oracle Cloud VPS or Quantum Terminal locally

**Need mobile-friendly access?**
→ Cloud Terminal (web) or Quantum Terminal (iOS/Android app)

## 📱 Mobile Support

### Cloud Terminal (Web)
- Responsive web design
- Quick command buttons (Ctrl+C, Esc, arrow keys, Clear, New Tab)
- Touch-friendly controls with persistent visibility
- Persistent sessions via tmux
- PWA support for app-like experience

### Quantum Terminal (Native Apps)
- Native iOS app with full terminal emulation
- Android app via NDK
- Hardware-accelerated 3D animations
- Touch-optimized input handling
- Local file system access

## 🛠️ Installation Scripts

### Quick VPS Setup (3 minutes)
```bash
# Run on any Ubuntu VPS:
curl -sSL https://raw.githubusercontent.com/your-repo/quick-setup.sh | bash
```

### Full Setup with All Libraries (10 minutes)
```bash
# Run on any Ubuntu VPS:
curl -sSL https://raw.githubusercontent.com/your-repo/full-setup.sh | bash
```

## 📊 Performance Characteristics

### Cloud Terminal
- **WebSocket latency**: <10ms typical
- **Session persistence**: tmux-based
- **Concurrent users**: Limited by VPS resources
- **Network overhead**: Minimal with compression

### Quantum Terminal
- **Rendering**: 60+ FPS with GPU acceleration
- **Input latency**: <1ms (native)
- **Memory usage**: <50MB base
- **CPU usage**: <5% idle with animations
- **Startup time**: <100ms

## 🔒 Security

- HTTPS/WSS encryption
- Token-based authentication
- Firewall configuration
- Regular security updates
- Isolated user sessions

## 🔄 Advanced Features

### Process Cleanup on Logout
Automatically kills all user processes when logging out to free up system resources:

- Kills all non-essential processes (Python, Node.js, editors, etc.)
- Clears system memory caches
- Frees up swap space
- Enabled by default when `ENABLE_REBOOT_ON_LOGOUT=true`
- Can be enabled separately with `ENABLE_PROCESS_CLEANUP=true`

### Reboot on Logout
For enhanced security or system cleanup, you can enable automatic system reboot when users log out:

```bash
# Run on your VPS as root/sudo:
sudo ./scripts/vps/enable-reboot-on-logout.sh
```

This feature:
- Kills all user processes before reboot
- Cleans up all session data
- Ensures fresh state for next user
- Requires `ENABLE_REBOOT_ON_LOGOUT=true` environment variable
- Only works on VPS deployments (not on managed platforms like Render/Vercel)

### Manual System Cleanup
For aggressive system cleanup without reboot:

```bash
# Run on your VPS as root/sudo:
sudo ./scripts/vps/aggressive-cleanup.sh
```

This will:
- Kill all non-essential processes
- Free up memory and swap
- Clean package manager caches
- Remove old logs and temp files
- Close unnecessary network connections

## 💰 Cost Comparison

| Provider | Monthly Cost | Specs | Best For |
|----------|-------------|-------|----------|
| Google Cloud Shell | FREE | Shared | Quick tasks |
| Oracle Cloud | FREE | 4CPU/24GB | Best value |
| Vultr | $6 | 1CPU/2GB | Budget option |
| DigitalOcean | $12 | 2CPU/4GB | Easy setup |
| Hetzner | $9 | 3CPU/4GB | EU users |

## 🚀 Get Started

### Fastest Option (Local)
```bash
git clone <repo>
cd quantum-terminal
./run.sh  # Launches native terminal with 3D animations
```

### Cloud Option (Remote)
1. **Instant**: Use https://shell.cloud.google.com
2. **Self-hosted**: Deploy to VPS + optional web frontend

## 📁 Project Structure

```
.
├── quantum-terminal/     # Native terminal emulator (C)
├── render-app/          # Cloud terminal web frontend
├── c-port/             # High-performance web server
├── scripts/            # Setup and deployment scripts
└── docs/              # Additional documentation
```

---

Built with ❤️ for developers who want beautiful, fast terminal access - whether local or remote.