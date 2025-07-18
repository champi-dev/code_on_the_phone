#!/bin/bash
# Run these commands on your DigitalOcean droplet to fix the terminal

# Check if ttyd is running
echo "Checking ttyd status..."
ps aux | grep ttyd

# Kill any existing ttyd processes
pkill ttyd

# Start ttyd with iframe-compatible configuration (keyboard input FIXED)
echo "Starting ttyd with iframe-compatible settings..."
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

# Check firewall
echo "Checking firewall..."
ufw status

# If firewall is blocking, open the port
ufw allow 7681/tcp

# Check if it's listening
echo "Checking if port 7681 is open..."
netstat -tlnp | grep 7681

echo "Your terminal should now be accessible at:"
echo "http://$(curl -s ipv4.icanhazip.com):7681"