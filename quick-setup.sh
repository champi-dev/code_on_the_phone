#!/bin/bash
# Quick Setup Script - Run this after SSHing into your Hetzner VPS
# This installs everything you need for the cloud terminal

set -euo pipefail

echo "🚀 Cloud Terminal Quick Setup"
echo "============================"

# Update system
echo "📦 Updating system packages..."
apt update && apt upgrade -y

# Install essential packages
echo "🔧 Installing essential tools..."
apt install -y \
    curl wget git build-essential cmake \
    zsh tmux neovim htop btop ncdu \
    python3-pip python3-dev python3-venv \
    nodejs npm golang rustc cargo \
    docker.io docker-compose \
    ripgrep fd-find bat exa jq httpie

# Install ttyd
echo "🖥️  Installing ttyd web terminal..."
wget https://github.com/tsl0922/ttyd/releases/download/1.7.4/ttyd.x86_64 -O /usr/local/bin/ttyd
chmod +x /usr/local/bin/ttyd

# Install Python libraries
echo "🐍 Installing Python libraries..."
pip3 install --upgrade pip
pip3 install \
    numpy scipy pandas matplotlib \
    scikit-learn torch tensorflow \
    jupyter ipython ptpython rich

# Install Node.js tools
echo "📦 Installing Node.js tools..."
npm install -g yarn pnpm typescript ts-node

# Set up Oh My Zsh
echo "🎨 Installing Oh My Zsh..."
sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# Install Starship prompt
echo "⭐ Installing Starship prompt..."
curl -sS https://starship.rs/install.sh | sh -s -- -y

# Configure ttyd service
echo "⚙️  Configuring ttyd service..."
cat > /etc/systemd/system/ttyd.service << 'EOF'
[Unit]
Description=ttyd Web Terminal
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/ttyd \
  -p 7681 \
  -t fontSize=16 \
  -t 'theme={"background": "#1a1b26", "foreground": "#c0caf5"}' \
  --check-origin=false \
  /usr/bin/tmux new -A -s main
Restart=always
User=root

[Install]
WantedBy=multi-user.target
EOF

# Configure firewall
echo "🔒 Configuring firewall..."
ufw allow 22/tcp
ufw allow 7681/tcp
ufw --force enable

# Start ttyd
systemctl daemon-reload
systemctl enable ttyd
systemctl start ttyd

# Create welcome message
cat > /etc/motd << 'EOF'
═══════════════════════════════════════════════════════════════
        🚀 Cloud Terminal - High Performance Shell
═══════════════════════════════════════════════════════════════
 
 Terminal URL: http://YOUR_IP:7681
 
 Quick Commands:
   python3    - Python REPL          btop    - System Monitor
   ipython    - Enhanced Python      tmux    - Terminal Multiplexer
   node       - Node.js REPL         nvim    - Neovim Editor
   
 All computational libraries are pre-installed!
 
═══════════════════════════════════════════════════════════════
EOF

# Get server IP
SERVER_IP=$(curl -s http://ipv4.icanhazip.com)

echo ""
echo "✅ Setup Complete!"
echo "=================="
echo "🌐 Terminal URL: http://$SERVER_IP:7681"
echo "📝 Next: Deploy the Render/Vercel frontend"
echo ""
echo "Test your terminal now by opening the URL above!"