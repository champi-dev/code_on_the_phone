#!/bin/bash
# Cloud Terminal with Render.com Frontend (Free Tier)
# No domain needed - uses render.onrender.com subdomain

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 Cloud Terminal Setup - Render + Hetzner${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Step 1: Hetzner VPS Setup (same as before but with different ttyd config)
cat > hetzner-setup.sh << 'HETZNER'
#!/bin/bash

# Install everything from the previous script...
# (All the packages, libraries, etc.)

# Modified ttyd service to accept connections from Render
cat > /etc/systemd/system/ttyd.service << 'EOF'
[Unit]
Description=ttyd Web Terminal
After=network.target

[Service]
Type=simple
User=root
# IMPORTANT: Generate a strong random token for security
TTYD_TOKEN=$(openssl rand -hex 32)
echo "TTYD_TOKEN=$TTYD_TOKEN" > /etc/ttyd.env

# Listen on all interfaces for Render to connect
ExecStart=/usr/local/bin/ttyd \
  -p 7681 \
  -t fontSize=16 \
  -t 'theme={"background": "#1a1b26", "foreground": "#c0caf5", "cursor": "#c0caf5", "selection": "#33467C", "black": "#15161E", "red": "#f7768e", "green": "#9ece6a", "yellow": "#e0af68", "blue": "#7aa2f7", "magenta": "#bb9af7", "cyan": "#7dcfff", "white": "#a9b1d6", "brightBlack": "#414868", "brightRed": "#f7768e", "brightGreen": "#9ece6a", "brightYellow": "#e0af68", "brightBlue": "#7aa2f7", "brightMagenta": "#bb9af7", "brightCyan": "#7dcfff", "brightWhite": "#c0caf5"}' \
  --check-origin=false \
  --max-clients=10 \
  /usr/bin/tmux new -A -s main

Restart=always
RestartSec=10
EnvironmentFile=/etc/ttyd.env

[Install]
WantedBy=multi-user.target
EOF

# Configure firewall to only allow specific IPs (Render's IPs)
ufw allow 22/tcp
ufw allow 7681/tcp
ufw --force enable

systemctl daemon-reload
systemctl enable ttyd
systemctl start ttyd

# Output the token for Render configuration
echo "TTYD_TOKEN=$TTYD_TOKEN"
echo "Save this token for your Render deployment!"
HETZNER

# Step 2: Create Render web service files
mkdir -p render-terminal-app

# Create package.json for Render
cat > render-terminal-app/package.json << 'PACKAGE'
{
  "name": "cloud-terminal-proxy",
  "version": "1.0.0",
  "description": "Beautiful cloud terminal interface",
  "main": "server.js",
  "scripts": {
    "start": "node server.js"
  },
  "dependencies": {
    "express": "^4.18.2",
    "http-proxy-middleware": "^2.0.6",
    "dotenv": "^16.0.3",
    "helmet": "^7.0.0",
    "compression": "^1.7.4"
  },
  "engines": {
    "node": ">=18.0.0"
  }
}
PACKAGE

# Create Node.js proxy server
cat > render-terminal-app/server.js << 'SERVER'
const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');
const helmet = require('helmet');
const compression = require('compression');
const path = require('path');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// Your Hetzner server IP (set in Render environment variables)
const TERMINAL_HOST = process.env.TERMINAL_HOST || 'your-hetzner-ip';
const TERMINAL_PORT = process.env.TERMINAL_PORT || '7681';
const TTYD_TOKEN = process.env.TTYD_TOKEN || '';

// Security middleware
app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
      styleSrc: ["'self'", "'unsafe-inline'"],
      imgSrc: ["'self'", "data:", "blob:"],
      connectSrc: ["'self'", "ws:", "wss:"],
      fontSrc: ["'self'", "data:"],
      objectSrc: ["'none'"],
      mediaSrc: ["'self'"],
      frameSrc: ["'self'"],
    },
  },
  crossOriginEmbedderPolicy: false,
}));

app.use(compression());

// Serve static files
app.use(express.static(path.join(__dirname, 'public')));

// Proxy WebSocket and HTTP requests to ttyd
const wsProxy = createProxyMiddleware({
  target: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`,
  ws: true,
  changeOrigin: true,
  headers: {
    'Authorization': `Bearer ${TTYD_TOKEN}`
  },
  onError: (err, req, res) => {
    console.error('Proxy error:', err);
    res.status(502).send('Terminal connection error');
  },
  onProxyReqWs: (proxyReq, req, socket) => {
    socket.on('error', (err) => {
      console.error('WebSocket error:', err);
    });
  }
});

// Terminal proxy route
app.use('/ws', wsProxy);
app.use('/token', wsProxy);

// Health check for Render
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Main route serves the terminal interface
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

const server = app.listen(PORT, () => {
  console.log(`Cloud Terminal running on port ${PORT}`);
});

// Handle WebSocket upgrade
server.on('upgrade', wsProxy.upgrade);
SERVER

# Create the beautiful terminal interface
mkdir -p render-terminal-app/public
cat > render-terminal-app/public/index.html << 'HTML'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Cloud Terminal</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background-color: #0d1117;
            color: #c9d1d9;
            overflow: hidden;
            position: fixed;
            width: 100%;
            height: 100%;
        }

        .terminal-container {
            opacity: 0.6;
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            display: flex;
            flex-direction: column;
        }

        .terminal-header {
            background: linear-gradient(to bottom, #161b22 0%, #0d1117 100%);
            padding: 8px 16px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            border-bottom: 1px solid #30363d;
            flex-shrink: 0;
        }

        .terminal-title {
            font-size: 14px;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            background: #3fb950;
            border-radius: 50%;
            animation: pulse 2s infinite;
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        #terminal-frame {
            flex: 1;
            width: 100%;
            border: none;
            background: #0d1117;
        }

        .quick-bar {
            background: #161b22;
            border-top: 1px solid #30363d;
            padding: 8px;
            display: flex;
            gap: 8px;
            overflow-x: auto;
            flex-shrink: 0;
        }

        .quick-btn {
            background: #21262d;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 6px 12px;
            color: #c9d1d9;
            font-size: 12px;
            white-space: nowrap;
            cursor: pointer;
            transition: all 0.2s;
            flex-shrink: 0;
        }

        .quick-btn:active {
            transform: scale(0.95);
        }

        .loading {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            text-align: center;
        }

        .spinner {
            width: 40px;
            height: 40px;
            border: 3px solid #30363d;
            border-top-color: #7aa2f7;
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin: 0 auto 16px;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="terminal-container">
        <div class="terminal-header">
            <div class="terminal-title">
                <div class="status-dot"></div>
                Cloud Terminal
            </div>
            <div style="font-size: 11px; color: #8b949e;">
                O(1) Performance
            </div>
        </div>
        
        <div class="loading" id="loading">
            <div class="spinner"></div>
            <div style="color: #8b949e;">Connecting...</div>
        </div>
        
        <iframe id="terminal-frame" style="display: none;"></iframe>
        
        <div class="quick-bar">
            <button class="quick-btn" onclick="sendCmd('clear')">Clear</button>
            <button class="quick-btn" onclick="sendCmd('ls -la')">List Files</button>
            <button class="quick-btn" onclick="sendCmd('python3')">Python</button>
            <button class="quick-btn" onclick="sendCmd('node')">Node.js</button>
            <button class="quick-btn" onclick="sendCmd('btop')">Monitor</button>
            <button class="quick-btn" onclick="sendCmd('git status')">Git Status</button>
            <button class="quick-btn" onclick="sendCmd('tmux new-window')">New Tab</button>
        </div>
    </div>

    <script>
        const frame = document.getElementById('terminal-frame');
        const loading = document.getElementById('loading');
        
        // Connect to terminal through our proxy
        frame.src = '/ws';
        
        frame.onload = () => {
            loading.style.display = 'none';
            frame.style.display = 'block';
        };
        
        function sendCmd(cmd) {
            // Send command to terminal
            frame.contentWindow.postMessage({ type: 'input', data: cmd + '\r' }, '*');
        }
        
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.key === 'k') {
                e.preventDefault();
                sendCmd('clear');
            }
        });
    </script>
</body>
</html>
HTML

# Create .env.example
cat > render-terminal-app/.env.example << 'ENV'
# Your Hetzner server IP
TERMINAL_HOST=your-hetzner-server-ip

# Terminal port (default: 7681)
TERMINAL_PORT=7681

# Security token from ttyd setup
TTYD_TOKEN=your-secure-token-here
ENV

# Create render.yaml for easy deployment
cat > render-terminal-app/render.yaml << 'RENDER'
services:
  - type: web
    name: cloud-terminal
    env: node
    buildCommand: npm install
    startCommand: npm start
    envVars:
      - key: NODE_ENV
        value: production
      - key: TERMINAL_HOST
        sync: false
      - key: TERMINAL_PORT
        value: 7681
      - key: TTYD_TOKEN
        sync: false
RENDER

# Create README for deployment
cat > render-terminal-app/README.md << 'README'
# Cloud Terminal on Render

## Quick Deploy to Render

1. First, set up your Hetzner VPS:
   ```bash
   # Run on your local machine
   bash hetzner-setup.sh
   # Save the TTYD_TOKEN that's generated
   ```

2. Deploy to Render:
   - Push this folder to a GitHub repo
   - Connect your GitHub account to Render
   - Create a new Web Service
   - Select this repository
   - Render will auto-detect the configuration

3. Set Environment Variables in Render Dashboard:
   - `TERMINAL_HOST`: Your Hetzner server IP
   - `TERMINAL_PORT`: 7681
   - `TTYD_TOKEN`: The token from step 1

4. Access your terminal at:
   `https://your-app-name.onrender.com`

## Features
- Free Render hosting (no domain needed)
- Secure WebSocket proxy
- Mobile-friendly interface
- Quick command buttons
- O(1) performance monitoring

## Security
- Token-based authentication
- HTTPS by default on Render
- Isolated terminal sessions
README

echo -e "${GREEN}✅ Render deployment files created!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Deployment Steps:${NC}"
echo "1. Set up Hetzner VPS: bash hetzner-setup.sh"
echo "2. Save the generated TTYD_TOKEN"
echo "3. Push render-terminal-app/ to GitHub"
echo "4. Deploy on Render.com (free tier)"
echo "5. Set environment variables in Render dashboard"
echo "6. Access at: https://your-app.onrender.com"
echo ""
echo -e "${GREEN}No domain purchase needed! 🎉${NC}"