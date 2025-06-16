#!/bin/bash
# Direct installation script - copy and paste this into your terminal

cat > /tmp/setup.sh << 'SCRIPT'
#!/bin/bash
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
pip3 install numpy pandas jupyter ipython scikit-learn torch

# Install Oh My Zsh
echo -e "${GREEN}🎨 Installing Oh My Zsh...${NC}"
sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# Install Starship
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
ExecStart=/usr/local/bin/ttyd -p 7681 -t fontSize=16 --check-origin=false /usr/bin/tmux new -A -s main
Restart=always

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable ttyd
systemctl start ttyd

# Configure firewall
echo -e "${GREEN}🔒 Configuring firewall...${NC}"
ufw allow 22/tcp
ufw allow 7681/tcp
ufw --force enable

# Get IP
SERVER_IP=$(curl -s http://ipv4.icanhazip.com)

echo ""
echo -e "${GREEN}✅ Setup Complete!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "🌐 Terminal URL: ${GREEN}http://$SERVER_IP:7681${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
SCRIPT

bash /tmp/setup.sh