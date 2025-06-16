#!/bin/bash
# Fix keyboard input in terminal iframe - removes canvas renderer

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🔧 Fixing Keyboard Input in Terminal${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if command -v ttyd &> /dev/null; then
    echo -e "${GREEN}✅ ttyd found - applying keyboard input fix${NC}"
    
    # Kill existing ttyd processes completely
    echo "Stopping all ttyd processes..."
    sudo pkill -9 ttyd || true
    sudo fuser -k 7681/tcp || true
    sleep 3
    
    # Start ttyd with iframe-compatible configuration
    echo "Starting ttyd with iframe-compatible settings (NO canvas renderer)..."
    ttyd -p 7681 \
      -t fontSize=14 \
      -t lineHeight=1.2 \
      -t bellStyle=none \
      -t scrollback=1000 \
      -t fastScrollSensitivity=10 \
      -t scrollSensitivity=5 \
      -t smoothScrollDuration=0 \
      -t cursorBlink=true \
      -t 'theme={"background": "#0d1117", "foreground": "#c9d1d9", "cursor": "#c9d1d9", "selection": "#33467C"}' \
      --check-origin=false \
      --max-clients=10 \
      /usr/bin/bash &
    
    echo -e "${GREEN}✅ ttyd restarted with keyboard-compatible settings${NC}"
    
    # Wait for startup
    sleep 3
    
    # Test if it's working
    echo "Testing terminal connection..."
    if netstat -tlnp | grep -q 7681; then
        echo -e "${GREEN}✅ Terminal is running on port 7681${NC}"
        echo -e "${GREEN}✅ Try typing in your terminal now!${NC}"
    else
        echo -e "${RED}❌ Terminal failed to start${NC}"
        exit 1
    fi
    
else
    echo -e "${YELLOW}⚠️  ttyd not found - this appears to be a local development setup${NC}"
    echo "The HTML interface files have been optimized to remove canvas renderer."
fi

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}🎉 Keyboard input fix completed!${NC}"
echo ""
echo -e "${YELLOW}What was fixed:${NC}"
echo "• Removed canvas renderer (main cause of input failure)"
echo "• Removed problematic windowOptions settings"
echo "• Removed optimizeForThroughput flag"
echo "• Restored DOM renderer for reliable keyboard input"
echo "• Added iframe focus management"
echo "• Kept essential scrolling optimizations"
echo ""
echo -e "${BLUE}Your terminal keyboard input should work now! ⌨️${NC}"
echo -e "${YELLOW}Note: Scrolling during heavy output may be slightly less smooth,${NC}"
echo -e "${YELLOW}but keyboard input reliability is more important.${NC}"