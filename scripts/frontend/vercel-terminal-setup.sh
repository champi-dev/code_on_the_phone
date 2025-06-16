#!/bin/bash
# Alternative: Cloud Terminal with Vercel Frontend (Free Tier)
# Uses vercel.app subdomain - no custom domain needed

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 Cloud Terminal Setup - Vercel + Hetzner${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Create Vercel app directory
mkdir -p vercel-terminal-app

# Create package.json
cat > vercel-terminal-app/package.json << 'PACKAGE'
{
  "name": "cloud-terminal-vercel",
  "version": "1.0.0",
  "scripts": {
    "dev": "next dev",
    "build": "next build",
    "start": "next start"
  },
  "dependencies": {
    "next": "14.0.0",
    "react": "18.2.0",
    "react-dom": "18.2.0"
  }
}
PACKAGE

# Create Next.js API route for proxying
mkdir -p vercel-terminal-app/pages/api
cat > vercel-terminal-app/pages/api/terminal.js << 'API'
export default async function handler(req, res) {
  // This handles the WebSocket upgrade
  const TERMINAL_HOST = process.env.TERMINAL_HOST;
  const TERMINAL_PORT = process.env.TERMINAL_PORT || '7681';
  
  // For Vercel, we'll use an iframe approach instead of WebSocket proxy
  // due to Vercel's limitations with WebSocket connections
  res.status(200).json({ 
    terminalUrl: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`,
    status: 'ready' 
  });
}
API

# Create the main terminal page
cat > vercel-terminal-app/pages/index.js << 'PAGE'
import { useEffect, useState } from 'react';
import Head from 'next/head';

export default function Terminal() {
  const [terminalUrl, setTerminalUrl] = useState('');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    // For Vercel, we'll create a direct connection using a secure tunnel
    // You'll need to set up a tunnel service like Cloudflare Tunnel or Tailscale
    const connectTerminal = async () => {
      try {
        // Use your tunnel URL here (e.g., from Cloudflare Tunnel)
        const tunnelUrl = process.env.NEXT_PUBLIC_TUNNEL_URL || 'https://terminal.your-tunnel.com';
        setTerminalUrl(tunnelUrl);
        setLoading(false);
      } catch (error) {
        console.error('Failed to connect:', error);
      }
    };
    
    connectTerminal();
  }, []);

  return (
    <>
      <Head>
        <title>Cloud Terminal</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
      </Head>
      
      <div className="terminal-container">
        <div className="terminal-header">
          <div className="terminal-title">
            <div className="status-dot"></div>
            Cloud Terminal
            <span className="perf-badge">O(1)</span>
          </div>
        </div>
        
        {loading ? (
          <div className="loading">
            <div className="spinner"></div>
            <div>Connecting to terminal...</div>
          </div>
        ) : (
          <iframe
            id="terminal-frame"
            src={terminalUrl}
            frameBorder="0"
            style={{ width: '100%', height: 'calc(100vh - 100px)' }}
          />
        )}
        
        <div className="quick-bar">
          <button onClick={() => sendCommand('clear')}>Clear</button>
          <button onClick={() => sendCommand('python3')}>Python</button>
          <button onClick={() => sendCommand('node')}>Node.js</button>
          <button onClick={() => sendCommand('git status')}>Git</button>
          <button onClick={() => sendCommand('btop')}>Monitor</button>
        </div>
      </div>
      
      <style jsx global>{\`
        * {
          margin: 0;
          padding: 0;
          box-sizing: border-box;
        }
        
        body {
          font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
          background: #0d1117;
          color: #c9d1d9;
          overflow: hidden;
        }
        
        .terminal-container {
          opacity: 0.6;
          height: 100vh;
          display: flex;
          flex-direction: column;
        }
        
        .terminal-header {
          background: linear-gradient(to bottom, #161b22 0%, #0d1117 100%);
          padding: 12px 16px;
          display: flex;
          align-items: center;
          justify-content: space-between;
          border-bottom: 1px solid #30363d;
        }
        
        .terminal-title {
          display: flex;
          align-items: center;
          gap: 8px;
          font-weight: 600;
        }
        
        .status-dot {
          width: 8px;
          height: 8px;
          background: #3fb950;
          border-radius: 50%;
          animation: pulse 2s infinite;
        }
        
        .perf-badge {
          background: #1a1b26;
          border: 1px solid #30363d;
          border-radius: 4px;
          padding: 2px 8px;
          font-size: 11px;
          color: #9ece6a;
          margin-left: 8px;
        }
        
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
        
        .loading {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          height: 100%;
          gap: 16px;
          color: #8b949e;
        }
        
        .spinner {
          width: 40px;
          height: 40px;
          border: 3px solid #30363d;
          border-top-color: #7aa2f7;
          border-radius: 50%;
          animation: spin 1s linear infinite;
        }
        
        @keyframes spin {
          to { transform: rotate(360deg); }
        }
        
        .quick-bar {
          background: #161b22;
          border-top: 1px solid #30363d;
          padding: 8px;
          display: flex;
          gap: 8px;
          overflow-x: auto;
        }
        
        .quick-bar button {
          background: #21262d;
          border: 1px solid #30363d;
          border-radius: 6px;
          padding: 6px 12px;
          color: #c9d1d9;
          font-size: 12px;
          cursor: pointer;
          white-space: nowrap;
          transition: all 0.2s;
        }
        
        .quick-bar button:active {
          transform: scale(0.95);
        }
      \`}</style>
    </>
  );
  
  function sendCommand(cmd) {
    const frame = document.getElementById('terminal-frame');
    if (frame && frame.contentWindow) {
      frame.contentWindow.postMessage({ type: 'input', data: cmd + '\\r' }, '*');
    }
  }
}
PAGE

# Create vercel.json
cat > vercel-terminal-app/vercel.json << 'VERCEL'
{
  "framework": "nextjs",
  "env": {
    "NEXT_PUBLIC_TUNNEL_URL": "@tunnel-url"
  }
}
VERCEL

# Create Cloudflare Tunnel setup script
cat > setup-cloudflare-tunnel.sh << 'TUNNEL'
#!/bin/bash
# Setup Cloudflare Tunnel for secure access without exposing ports

# Install cloudflared
curl -L https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -o /usr/local/bin/cloudflared
chmod +x /usr/local/bin/cloudflared

# Create tunnel (you'll need to login to Cloudflare)
cloudflared tunnel login
cloudflared tunnel create cloud-terminal

# Configure tunnel
cat > ~/.cloudflared/config.yml << EOF
tunnel: cloud-terminal
credentials-file: /root/.cloudflared/<tunnel-id>.json

ingress:
  - hostname: terminal.example.com
    service: http://localhost:7681
  - service: http_status:404
EOF

# Install as service
cloudflared service install
systemctl start cloudflared

echo "Tunnel URL will be: https://terminal.your-tunnel-id.trycloudflare.com"
TUNNEL

# Create alternative Tailscale setup
cat > setup-tailscale.sh << 'TAILSCALE'
#!/bin/bash
# Alternative: Use Tailscale for secure access

# Install Tailscale
curl -fsSL https://tailscale.com/install.sh | sh

# Start Tailscale
tailscale up

# Enable MagicDNS and HTTPS
tailscale cert $(hostname)

echo "Access your terminal via Tailscale network"
echo "URL: https://$(hostname).your-tailnet.ts.net:7681"
TAILSCALE

# Create deployment guide
cat > vercel-terminal-app/README.md << 'README'
# Cloud Terminal on Vercel

## Setup Options

### Option 1: Cloudflare Tunnel (Recommended)
1. On your Hetzner server, run:
   ```bash
   bash setup-cloudflare-tunnel.sh
   ```

2. Deploy to Vercel:
   ```bash
   vercel --prod
   ```

3. Set environment variable in Vercel:
   - `NEXT_PUBLIC_TUNNEL_URL`: Your Cloudflare tunnel URL

### Option 2: Tailscale VPN
1. Install Tailscale on both server and your devices
2. Use Tailscale URL in the app

### Option 3: Direct SSH Tunnel (Development)
```bash
# On your local machine
ssh -L 7681:localhost:7681 root@your-hetzner-ip
# Access at http://localhost:7681
```

## Features
- Free hosting on Vercel
- No domain purchase needed
- Secure tunnel connection
- Mobile-optimized interface
- Quick command shortcuts

## Deploy to Vercel
[![Deploy with Vercel](https://vercel.com/button)](https://vercel.com/new/clone?repository-url=https://github.com/your-repo/vercel-terminal-app)
README

echo -e "${GREEN}✅ Vercel deployment files created!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Deployment Options:${NC}"
echo ""
echo -e "${GREEN}Option 1: Render (Easier)${NC}"
echo "- Simple WebSocket proxy"
echo "- Deploy: render-terminal-app/"
echo "- URL: https://your-app.onrender.com"
echo ""
echo -e "${GREEN}Option 2: Vercel + Cloudflare Tunnel${NC}"
echo "- Better performance"
echo "- Deploy: vercel-terminal-app/"
echo "- URL: https://your-app.vercel.app"
echo ""
echo "Both options are FREE and don't require a domain! 🎉"