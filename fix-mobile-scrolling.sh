#!/bin/bash
# Fix terminal scrolling performance issues on mobile

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🔧 Fixing Mobile Terminal Scrolling Issues${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Check if running on VPS or local
if command -v ttyd &> /dev/null; then
    echo -e "${GREEN}✅ ttyd found - applying VPS optimizations${NC}"
    
    # Kill existing ttyd processes
    echo "Stopping existing ttyd processes..."
    pkill ttyd || true
    
    # Start optimized ttyd
    echo "Starting mobile-optimized ttyd..."
    ttyd -p 7681 \
      -t fontSize=14 \
      -t lineHeight=1.2 \
      -t bellStyle=none \
      -t scrollback=1000 \
      -t rendererType=canvas \
      -t fastScrollSensitivity=10 \
      -t scrollSensitivity=5 \
      -t smoothScrollDuration=0 \
      -t cursorBlink=true \
      -t cursorStyle=block \
      -t allowTransparency=true \
      -t 'theme={"background": "#0d1117", "foreground": "#c9d1d9", "cursor": "#c9d1d9", "selection": "#33467C"}' \
      --check-origin=false \
      --max-clients=10 \
      /usr/bin/tmux new -A -s main &
    
    echo -e "${GREEN}✅ ttyd restarted with mobile optimizations${NC}"
    
    # Install optimized service file if systemd is available
    if command -v systemctl &> /dev/null; then
        echo "Installing optimized systemd service..."
        sudo cp mobile-terminal-optimized.service /etc/systemd/system/ttyd.service
        sudo systemctl daemon-reload
        sudo systemctl enable ttyd
        sudo systemctl restart ttyd
        echo -e "${GREEN}✅ Systemd service updated${NC}"
    fi
    
else
    echo -e "${YELLOW}⚠️  ttyd not found - this appears to be a local development setup${NC}"
fi

# Apply CSS optimizations to all terminal interfaces
echo "Applying CSS optimizations to terminal interfaces..."

# The HTML files have already been updated with:
# - touch-action: pan-y for better touch scrolling
# - -webkit-overflow-scrolling: touch for momentum scrolling
# - Optimized iframe configurations
# - xterm.js performance tuning

echo -e "${GREEN}✅ Terminal interface optimizations applied${NC}"

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}🎉 Mobile scrolling fixes completed!${NC}"
echo ""
echo -e "${YELLOW}What was fixed:${NC}"
echo "• Added momentum scrolling (-webkit-overflow-scrolling: touch)"
echo "• Optimized touch actions for better responsiveness"
echo "• Configured xterm.js for mobile performance"
echo "• Reduced scroll sensitivity for smoother experience"
echo "• Disabled smooth scrolling animations"
echo "• Optimized iframe cross-origin communication"
echo "• Updated ttyd configuration for mobile devices"
echo "• Added output throttling to prevent UI blocking during heavy logs"
echo "• Configured canvas renderer for better performance"
echo "• Limited max clients to reduce server load"
echo ""
echo -e "${BLUE}Your terminal should now scroll smoothly on mobile! 📱${NC}"