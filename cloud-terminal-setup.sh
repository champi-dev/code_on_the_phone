#!/bin/bash
# Cloud Terminal Setup Script - Beautiful, Fast, Feature-Rich Terminal
# Budget: $20/month on Hetzner Cloud

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 Cloud Terminal Setup - High Performance Shell Environment${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Hetzner Cloud configuration
# CX21: 2 vCPU, 4GB RAM, 40GB NVMe SSD - €5.83/month (~$6.50)
# or CPX21: 3 vCPU (AMD), 4GB RAM, 80GB NVMe SSD - €8.21/month (~$9.20)
HETZNER_TOKEN="${HETZNER_TOKEN:-your_token_here}"
SERVER_TYPE="cpx21"  # Better performance, still under $20
LOCATION="fsn1"      # Falkenstein, Germany (lowest latency for most)
IMAGE="ubuntu-24.04"

# Function to set up the VPS
setup_vps() {
    echo -e "${GREEN}Step 1: Setting up Hetzner VPS...${NC}"
    
    # Create server using Hetzner CLI
    hcloud server create \
        --name cloud-terminal \
        --type "$SERVER_TYPE" \
        --image "$IMAGE" \
        --location "$LOCATION" \
        --ssh-key ~/.ssh/id_rsa.pub \
        --user-data-from-file cloud-init.yaml
}

# Create cloud-init configuration
cat > cloud-init.yaml << 'EOF'
#cloud-config
package_update: true
package_upgrade: true

# Install base packages
packages:
  - curl
  - wget
  - git
  - build-essential
  - cmake
  - pkg-config
  - libssl-dev
  - zsh
  - tmux
  - neovim
  - htop
  - btop
  - ncdu
  - ripgrep
  - fd-find
  - bat
  - exa
  - jq
  - httpie
  - mosh
  - python3-pip
  - python3-dev
  - python3-venv
  - nodejs
  - npm
  - golang
  - rustc
  - cargo
  - docker.io
  - docker-compose

# System optimizations
write_files:
  - path: /etc/sysctl.d/99-performance.conf
    content: |
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
      
      # Enable huge pages
      vm.nr_hugepages = 128

  - path: /etc/security/limits.d/99-performance.conf
    content: |
      * soft nofile 1048576
      * hard nofile 1048576
      * soft nproc 65536
      * hard nproc 65536

runcmd:
  # Apply sysctl settings
  - sysctl -p /etc/sysctl.d/99-performance.conf
  
  # Install Rust tools
  - curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
  - source $HOME/.cargo/env
  - cargo install starship tokei hyperfine bottom

  # Install development tools
  - pip3 install --upgrade pip
  - pip3 install numpy scipy sympy matplotlib pandas scikit-learn
  - pip3 install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu
  - pip3 install tensorflow cvxpy ortools networkx
  - pip3 install jupyter ipython ptpython rich httpx
  
  # Install Node.js tools
  - npm install -g yarn pnpm typescript ts-node
  - npm install -g @angular/cli @vue/cli create-react-app
  - npm install -g react-native-cli expo-cli
  
  # Install Flutter
  - git clone https://github.com/flutter/flutter.git -b stable /opt/flutter
  - echo 'export PATH="/opt/flutter/bin:$PATH"' >> /etc/profile.d/flutter.sh
  
  # Install Android command line tools
  - mkdir -p /opt/android-sdk
  - cd /opt/android-sdk
  - wget https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
  - unzip commandlinetools-linux-9477386_latest.zip
  - rm commandlinetools-linux-9477386_latest.zip
  - echo 'export ANDROID_HOME=/opt/android-sdk' >> /etc/profile.d/android.sh
  - echo 'export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"' >> /etc/profile.d/android.sh

  # Install ttyd (web terminal)
  - wget https://github.com/tsl0922/ttyd/releases/latest/download/ttyd.x86_64 -O /usr/local/bin/ttyd
  - chmod +x /usr/local/bin/ttyd
  
  # Install Octave (MATLAB alternative)
  - apt-get install -y octave octave-signal octave-control octave-image
  
  # Install SageMath dependencies
  - apt-get install -y sagemath
  
  # Install additional physics libraries
  - pip3 install pybullet box2d-py pymunk
  
  # Install CS algorithm libraries
  - pip3 install algorithms python-graph-core igraph
  - git clone https://github.com/keon/algorithms.git /opt/algorithms
  
  # Set up Oh My Zsh for default user
  - sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended
  
  # Configure tmux
  - git clone https://github.com/tmux-plugins/tpm ~/.tmux/plugins/tpm

EOF

# Create the terminal configuration script
cat > setup-terminal.sh << 'SCRIPT'
#!/bin/bash

# Install Oh My Zsh plugins
git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
git clone https://github.com/zsh-users/zsh-syntax-highlighting ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting
git clone https://github.com/zsh-users/zsh-completions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-completions

# Configure Starship prompt
mkdir -p ~/.config
cat > ~/.config/starship.toml << 'EOF'
format = """
[┌───────────────────>](bold green)
[│](bold green)$username$hostname$directory$git_branch$git_status$python$nodejs$rust$golang$docker_context
[└─>](bold green) """

[username]
style_user = "green bold"
style_root = "red bold"
format = "[$user]($style) "
disabled = false
show_always = true

[hostname]
ssh_only = false
format = "@ [$hostname](bold blue) "

[directory]
truncation_length = 3
truncate_to_repo = false
format = "in [$path](bold cyan) "

[git_branch]
format = "on [$symbol$branch]($style) "
style = "bold purple"

[git_status]
format = '([\[$all_status$ahead_behind\]]($style) )'
style = "bold red"

[python]
format = "via [${symbol}${pyenv_prefix}(${version} )(\($virtualenv\) )]($style)"

[nodejs]
format = "via [⬢ $version](bold green) "

[rust]
format = "via [🦀 $version](red bold) "

[golang]
format = "via [🐹 $version](bold cyan) "
EOF

# Configure .zshrc
cat > ~/.zshrc << 'EOF'
export ZSH="$HOME/.oh-my-zsh"
ZSH_THEME=""  # We use starship

plugins=(
    git
    docker
    kubectl
    python
    pip
    node
    npm
    rust
    golang
    tmux
    zsh-autosuggestions
    zsh-syntax-highlighting
    zsh-completions
)

source $ZSH/oh-my-zsh.sh

# Starship prompt
eval "$(starship init zsh)"

# Aliases
alias ls='exa --icons'
alias ll='exa -la --icons'
alias tree='exa --tree --icons'
alias cat='bat'
alias find='fd'
alias grep='rg'
alias top='btop'
alias vim='nvim'
alias py='ptpython'

# Performance: Preload frequently used commands
zmodload zsh/zprof
zmodload zsh/sched

# Enable Powerlevel10k instant prompt
if [[ -r "${XDG_CACHE_HOME:-$HOME/.cache}/p10k-instant-prompt-${(%):-%n}.zsh" ]]; then
  source "${XDG_CACHE_HOME:-$HOME/.cache}/p10k-instant-prompt-${(%):-%n}.zsh"
fi

# FZF integration
[ -f ~/.fzf.zsh ] && source ~/.fzf.zsh

# Optimized history
HISTSIZE=50000
SAVEHIST=50000
setopt HIST_EXPIRE_DUPS_FIRST
setopt HIST_IGNORE_DUPS
setopt HIST_IGNORE_SPACE
setopt HIST_VERIFY
setopt INC_APPEND_HISTORY

# Directory stack
setopt AUTO_PUSHD
setopt PUSHD_IGNORE_DUPS
setopt PUSHD_SILENT

# Completion caching for O(1) performance
zstyle ':completion:*' use-cache on
zstyle ':completion:*' cache-path ~/.zsh/cache

# Environment variables
export EDITOR='nvim'
export ANDROID_HOME=/opt/android-sdk
export PATH="$HOME/.cargo/bin:/opt/flutter/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

# Python virtual environment
export WORKON_HOME=$HOME/.virtualenvs
export VIRTUALENVWRAPPER_PYTHON=/usr/bin/python3

# Node.js optimizations
export NODE_OPTIONS="--max-old-space-size=4096"

# Rust optimizations
export RUSTFLAGS="-C target-cpu=native"

# Function to quickly create Python virtual environments
mkvenv() {
    python3 -m venv ~/.virtualenvs/$1
    source ~/.virtualenvs/$1/bin/activate
}

# Function to activate Python virtual environments
workon() {
    source ~/.virtualenvs/$1/bin/activate
}
EOF

# Configure tmux
cat > ~/.tmux.conf << 'EOF'
# List of plugins
set -g @plugin 'tmux-plugins/tpm'
set -g @plugin 'tmux-plugins/tmux-sensible'
set -g @plugin 'tmux-plugins/tmux-resurrect'
set -g @plugin 'tmux-plugins/tmux-continuum'
set -g @plugin 'tmux-plugins/tmux-yank'
set -g @plugin 'tmux-plugins/tmux-prefix-highlight'
set -g @plugin 'dracula/tmux'

# Dracula theme configuration
set -g @dracula-plugins "cpu-usage ram-usage network time"
set -g @dracula-show-powerline true
set -g @dracula-show-flags true
set -g @dracula-refresh-rate 2
set -g @dracula-military-time true

# Enable true colors
set -g default-terminal "screen-256color"
set -ga terminal-overrides ",*256col*:Tc"

# Set prefix to Ctrl+Space
unbind C-b
set -g prefix C-Space
bind C-Space send-prefix

# Enable mouse
set -g mouse on

# Vi mode
setw -g mode-keys vi

# Split panes using | and -
bind | split-window -h
bind - split-window -v
unbind '"'
unbind %

# Fast pane switching
bind -n M-Left select-pane -L
bind -n M-Right select-pane -R
bind -n M-Up select-pane -U
bind -n M-Down select-pane -D

# Reload config
bind r source-file ~/.tmux.conf \; display-message "Config reloaded!"

# History
set -g history-limit 50000

# No delay for escape key
set -sg escape-time 0

# Window numbering
set -g base-index 1
setw -g pane-base-index 1
set -g renumber-windows on

# Initialize TMUX plugin manager
run '~/.tmux/plugins/tpm/tpm'
EOF

# Install FZF
git clone --depth 1 https://github.com/junegunn/fzf.git ~/.fzf
~/.fzf/install --all

# Install Neovim configuration (NvChad for beautiful and fast IDE experience)
git clone https://github.com/NvChad/NvChad ~/.config/nvim --depth 1

SCRIPT

# Create systemd service for ttyd
cat > ttyd.service << 'SERVICE'
[Unit]
Description=ttyd Web Terminal
After=network.target

[Service]
Type=simple
User=root
ExecStart=/usr/local/bin/ttyd -p 7681 -c user:password -t fontSize=16 -t 'theme={"background": "#1a1b26", "foreground": "#c0caf5", "cursor": "#c0caf5", "selection": "#33467C", "black": "#15161E", "red": "#f7768e", "green": "#9ece6a", "yellow": "#e0af68", "blue": "#7aa2f7", "magenta": "#bb9af7", "cyan": "#7dcfff", "white": "#a9b1d6", "brightBlack": "#414868", "brightRed": "#f7768e", "brightGreen": "#9ece6a", "brightYellow": "#e0af68", "brightBlue": "#7aa2f7", "brightMagenta": "#bb9af7", "brightCyan": "#7dcfff", "brightWhite": "#c0caf5"}' /usr/bin/tmux new -A -s main
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
SERVICE

# Create nginx configuration for SSL termination
cat > nginx-terminal.conf << 'NGINX'
server {
    listen 443 ssl http2;
    server_name your-domain.com;

    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;

    # Security headers
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;

    location / {
        proxy_pass http://127.0.0.1:7681;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # WebSocket support
        proxy_read_timeout 86400;
        
        # Mobile-friendly viewport
        sub_filter '</head>' '<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no"></head>';
        sub_filter_once on;
    }
}

server {
    listen 80;
    server_name your-domain.com;
    return 301 https://$server_name$request_uri;
}
NGINX

echo -e "${GREEN}✨ Cloud Terminal Setup Script Created!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Instructions:${NC}"
echo "1. Set your Hetzner API token: export HETZNER_TOKEN='your-token'"
echo "2. Run: bash cloud-terminal-setup.sh"
echo "3. SSH into your server and run: bash setup-terminal.sh"
echo "4. Configure SSL with Let's Encrypt for your domain"
echo "5. Access your terminal at https://your-domain.com"
echo ""
echo -e "${GREEN}Features:${NC}"
echo "• Beautiful Tokyo Night themed terminal"
echo "• All computational libraries pre-installed"
echo "• O(1) command completion with caching"
echo "• Mobile-friendly responsive design"
echo "• Persistent tmux sessions"
echo "• Under $20/month with Hetzner CPX21"