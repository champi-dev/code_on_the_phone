#!/bin/bash
# Full Setup Script - Installs ALL computational libraries and optimizations
# Takes ~10 minutes but includes everything you requested

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 Cloud Terminal Full Setup - High Performance Edition${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# System optimizations
echo -e "${GREEN}⚡ Applying performance optimizations...${NC}"
cat > /etc/sysctl.d/99-performance.conf << 'EOF'
# Network optimizations
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728
net.core.netdev_max_backlog = 5000
net.ipv4.tcp_congestion_control = bbr

# Memory optimizations
vm.swappiness = 10
vm.vfs_cache_pressure = 50
vm.nr_hugepages = 128
EOF
sysctl -p /etc/sysctl.d/99-performance.conf

# Update system
echo -e "${GREEN}📦 Updating system packages...${NC}"
apt-get update && apt-get upgrade -y

# Install development tools
echo -e "${GREEN}🔧 Installing development tools...${NC}"
apt-get install -y \
    curl wget git build-essential cmake pkg-config \
    zsh tmux neovim htop btop ncdu \
    python3-pip python3-dev python3-venv \
    nodejs npm golang rustc cargo \
    docker.io docker-compose \
    ripgrep fd-find bat exa fzf jq httpie \
    libssl-dev libffi-dev libblas-dev liblapack-dev

# Install computational libraries
echo -e "${GREEN}🧮 Installing computational libraries...${NC}"

# Python scientific stack
pip3 install --upgrade pip setuptools wheel
pip3 install \
    numpy scipy pandas matplotlib seaborn \
    scikit-learn scikit-image statsmodels \
    sympy cvxpy ortools networkx \
    jupyter jupyterlab ipython ptpython rich

# Machine Learning
pip3 install \
    torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu \
    tensorflow keras \
    xgboost lightgbm catboost

# Physics libraries
pip3 install pybullet box2d-py pymunk vpython

# Install Octave (MATLAB alternative)
echo -e "${GREEN}🔢 Installing Octave...${NC}"
apt-get install -y octave octave-signal octave-control octave-image

# Install SageMath
echo -e "${GREEN}🎯 Installing SageMath...${NC}"
apt-get install -y sagemath

# Install Node.js tools
echo -e "${GREEN}📦 Installing Node.js ecosystem...${NC}"
npm install -g yarn pnpm typescript ts-node nodemon pm2

# Install Flutter
echo -e "${GREEN}📱 Installing Flutter...${NC}"
git clone https://github.com/flutter/flutter.git -b stable /opt/flutter
echo 'export PATH="/opt/flutter/bin:$PATH"' >> /etc/profile.d/flutter.sh

# Install Android SDK
echo -e "${GREEN}🤖 Installing Android SDK...${NC}"
mkdir -p /opt/android-sdk
cd /opt/android-sdk
wget -q https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
unzip -q commandlinetools-linux-9477386_latest.zip
rm commandlinetools-linux-9477386_latest.zip
echo 'export ANDROID_HOME=/opt/android-sdk' >> /etc/profile.d/android.sh
echo 'export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"' >> /etc/profile.d/android.sh

# Install ttyd
echo -e "${GREEN}🖥️  Installing web terminal...${NC}"
wget -q https://github.com/tsl0922/ttyd/releases/latest/download/ttyd.x86_64 -O /usr/local/bin/ttyd
chmod +x /usr/local/bin/ttyd

# Install Rust tools
echo -e "${GREEN}🦀 Installing Rust tools...${NC}"
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source $HOME/.cargo/env
cargo install starship tokei hyperfine bottom du-dust

# Set up Oh My Zsh
echo -e "${GREEN}🎨 Installing Oh My Zsh...${NC}"
sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# Install Oh My Zsh plugins
git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
git clone https://github.com/zsh-users/zsh-syntax-highlighting ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting

# Configure zsh
cat > ~/.zshrc << 'EOF'
export ZSH="$HOME/.oh-my-zsh"
ZSH_THEME=""

plugins=(
    git docker python pip node npm
    zsh-autosuggestions
    zsh-syntax-highlighting
)

source $ZSH/oh-my-zsh.sh

# Starship prompt
eval "$(starship init zsh)"

# Aliases
alias ls='exa --icons'
alias ll='exa -la --icons'
alias cat='bat'
alias find='fd'
alias grep='rg'
alias top='btop'

# Environment
export EDITOR='nvim'
export PATH="$HOME/.cargo/bin:/opt/flutter/bin:$PATH"

# Performance: Enable zsh completion caching
zstyle ':completion:*' use-cache on
zstyle ':completion:*' cache-path ~/.zsh/cache
EOF

# Configure Starship
mkdir -p ~/.config
cat > ~/.config/starship.toml << 'EOF'
[time]
disabled = false
format = '[$time]($style) '

[memory_usage]
disabled = false
threshold = -1
format = "via $symbol [${ram}]($style) "

[nodejs]
format = "via [⬢ $version](bold green) "

[python]
format = 'via [${symbol}${pyenv_prefix}(${version} )(\($virtualenv\) )]($style)'
EOF

# Configure ttyd service with auth
TTYD_PASSWORD=$(openssl rand -base64 12)
cat > /etc/systemd/system/ttyd.service << EOF
[Unit]
Description=ttyd Web Terminal
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/ttyd \\
  -p 7681 \\
  -c user:$TTYD_PASSWORD \\
  -t fontSize=16 \\
  -t 'theme={"background": "#1a1b26", "foreground": "#c0caf5"}' \\
  --check-origin=false \\
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
ufw allow 22/tcp
ufw allow 7681/tcp
ufw --force enable

# Get server IP
SERVER_IP=$(curl -s http://ipv4.icanhazip.com)

# Final message
echo ""
echo -e "${GREEN}✅ Full Setup Complete!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "🌐 Terminal URL: ${GREEN}http://$SERVER_IP:7681${NC}"
echo -e "🔑 Login: ${YELLOW}user / $TTYD_PASSWORD${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "Installed: Python, Node.js, Go, Rust, Flutter, Android SDK"
echo "Libraries: NumPy, PyTorch, TensorFlow, Octave, SageMath, and more!"