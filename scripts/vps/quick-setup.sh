#!/bin/bash
# Universal Quick Setup Script - Works on any Ubuntu/Debian VPS
# Installs terminal with essential tools in ~3 minutes

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 Cloud Terminal Quick Setup${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Update system
echo -e "${GREEN}📦 Updating system...${NC}"
apt-get update && apt-get upgrade -y

# Install core packages
echo -e "${GREEN}🔧 Installing essential tools...${NC}"
apt-get install -y \
    curl wget git build-essential \
    zsh tmux neovim htop \
    python3-pip nodejs npm \
    ripgrep fd-find bat

# Install ttyd web terminal
echo -e "${GREEN}🖥️  Installing web terminal...${NC}"
wget -q https://github.com/tsl0922/ttyd/releases/latest/download/ttyd.x86_64 -O /usr/local/bin/ttyd
chmod +x /usr/local/bin/ttyd

# Install Python essentials
echo -e "${GREEN}🐍 Installing Python libraries...${NC}"
pip3 install --upgrade pip
pip3 install numpy pandas jupyter ipython

# Set up Oh My Zsh
echo -e "${GREEN}🎨 Installing Oh My Zsh...${NC}"
sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# Install Starship prompt
echo -e "${GREEN}⭐ Installing Starship prompt...${NC}"
curl -sS https://starship.rs/install.sh | sh -s -- -y

# Configure ttyd service
echo -e "${GREEN}⚙️  Configuring web terminal service...${NC}"
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

[Install]
WantedBy=multi-user.target
EOF

# Start services
systemctl daemon-reload
systemctl enable ttyd
systemctl start ttyd

# Configure firewall
echo -e "${GREEN}🔒 Configuring firewall...${NC}"
ufw allow 22/tcp
ufw allow 7681/tcp
ufw --force enable

# Get server IP
SERVER_IP=$(curl -s http://ipv4.icanhazip.com)

# Create welcome message
cat > /etc/motd << EOF
═══════════════════════════════════════════════════════════════
        🚀 Cloud Terminal Ready!
═══════════════════════════════════════════════════════════════
 Terminal URL: http://$SERVER_IP:7681
 
 Quick Commands:
   python3    - Python REPL
   ipython    - Enhanced Python  
   tmux       - Terminal Multiplexer
   nvim       - Neovim Editor
═══════════════════════════════════════════════════════════════
EOF

echo ""
echo -e "${GREEN}✅ Setup Complete!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "🌐 Terminal URL: ${GREEN}http://$SERVER_IP:7681${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"