#!/bin/bash
# Run these commands on your DigitalOcean droplet to fix the terminal

# Check if ttyd is running
echo "Checking ttyd status..."
ps aux | grep ttyd

# Kill any existing ttyd processes
pkill ttyd

# Start ttyd properly
echo "Starting ttyd..."
ttyd -p 7681 /usr/bin/bash &

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