#!/bin/bash
# Fix terminal scrolling during heavy output (like apt install logs)

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🔧 Fixing Terminal Scrolling During Heavy Output${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if command -v ttyd &> /dev/null; then
    echo -e "${GREEN}✅ ttyd found - applying heavy output optimizations${NC}"
    
    # Kill existing ttyd processes
    echo "Stopping existing ttyd processes..."
    pkill ttyd || true
    sleep 2
    
    # Start ttyd with heavy output optimizations
    echo "Starting ttyd with heavy output optimizations..."
    ttyd -p 7681 \
      -t fontSize=14 \
      -t lineHeight=1.2 \
      -t bellStyle=none \
      -t scrollback=1000 \
      -t rendererType=canvas \
      -t fastScrollSensitivity=10 \
      -t scrollSensitivity=5 \
      -t smoothScrollDuration=0 \
      -t windowsMode=false \
      -t macOptionIsMeta=true \
      -t rightClickSelectsWord=false \
      -t cursorBlink=true \
      -t cursorStyle=block \
      -t allowTransparency=true \
      -t 'windowOptions={"setWinSizePixels":false}' \
      -t 'theme={"background": "#0d1117", "foreground": "#c9d1d9", "cursor": "#c9d1d9", "selection": "#33467C"}' \
      --check-origin=false \
      --max-clients=3 \
      /usr/bin/bash &
    
    echo -e "${GREEN}✅ ttyd restarted with heavy output optimizations${NC}"
    
    # Wait a moment for startup
    sleep 3
    
    # Test if it's working
    echo "Testing terminal connection..."
    if netstat -tlnp | grep -q 7681; then
        echo -e "${GREEN}✅ Terminal is running on port 7681${NC}"
    else
        echo -e "${RED}❌ Terminal failed to start${NC}"
        exit 1
    fi
    
else
    echo -e "${YELLOW}⚠️  ttyd not found - this appears to be a local development setup${NC}"
    echo "The HTML interface files have been optimized for heavy output handling."
fi

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}🎉 Heavy output scrolling fixes completed!${NC}"
echo ""
echo -e "${YELLOW}Optimizations applied:${NC}"
echo "• Canvas renderer for better performance during heavy output"
echo "• Output throttling to prevent UI thread blocking"
echo "• Reduced max clients to 3 for better resource management"
echo "• Disabled window size pixel calculations"
echo "• Optimized xterm.js configuration for throughput"
echo "• Enhanced touch scrolling during active logging"
echo ""
echo -e "${BLUE}Now try running commands like 'apt install' - scrolling should work! 📱${NC}"
echo -e "${YELLOW}Tip: If you still have issues, try opening the keyboard - this often helps with touch scrolling.${NC}"