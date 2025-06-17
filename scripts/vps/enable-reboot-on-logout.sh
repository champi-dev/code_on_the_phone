#!/bin/bash
# Script to enable reboot on logout functionality
# This must be run on the VPS server with root/sudo privileges

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🔄 Configuring Reboot on Logout${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Check if running as root or with sudo
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}This script must be run as root or with sudo${NC}" 
   exit 1
fi

# Get the user running the Node.js app (usually 'cloud' or the deployment user)
read -p "Enter the username running the Node.js app (default: cloud): " APP_USER
APP_USER=${APP_USER:-cloud}

# Create sudoers entry to allow the app user to reboot without password
echo -e "${GREEN}📝 Adding sudoers entry for ${APP_USER}...${NC}"
cat > /etc/sudoers.d/allow-reboot << EOF
# Allow the application user to reboot the system
${APP_USER} ALL=(ALL) NOPASSWD: /sbin/reboot, /usr/sbin/reboot, /bin/systemctl reboot
EOF

# Set proper permissions
chmod 0440 /etc/sudoers.d/allow-reboot

# Verify sudoers syntax
if visudo -c -f /etc/sudoers.d/allow-reboot; then
    echo -e "${GREEN}✅ Sudoers configuration valid${NC}"
else
    echo -e "${RED}❌ Sudoers configuration invalid, removing...${NC}"
    rm /etc/sudoers.d/allow-reboot
    exit 1
fi

# Update systemd service if ttyd.service exists
if [ -f /etc/systemd/system/ttyd.service ]; then
    echo -e "${GREEN}🔧 Updating ttyd service environment...${NC}"
    # Add environment variable to the service
    if ! grep -q "Environment=\"ENABLE_REBOOT_ON_LOGOUT=true\"" /etc/systemd/system/ttyd.service; then
        sed -i '/\[Service\]/a Environment="ENABLE_REBOOT_ON_LOGOUT=true"' /etc/systemd/system/ttyd.service
        systemctl daemon-reload
    fi
fi

# Create or update .env file for the render app
if [ -d /opt/render-app ] || [ -d ~/render-app ]; then
    APP_DIR=$([ -d /opt/render-app ] && echo "/opt/render-app" || echo "~/render-app")
    echo -e "${GREEN}📄 Updating ${APP_DIR}/.env...${NC}"
    
    if [ -f "${APP_DIR}/.env" ]; then
        # Remove existing ENABLE_REBOOT_ON_LOGOUT if present
        sed -i '/^ENABLE_REBOOT_ON_LOGOUT=/d' "${APP_DIR}/.env"
    fi
    
    # Add the environment variable
    echo "ENABLE_REBOOT_ON_LOGOUT=true" >> "${APP_DIR}/.env"
    
    # Set proper ownership
    chown ${APP_USER}:${APP_USER} "${APP_DIR}/.env" 2>/dev/null || true
fi

echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✅ Reboot on logout has been configured!${NC}"
echo
echo -e "${BLUE}📌 Important Notes:${NC}"
echo -e "  • The system will reboot when users log out"
echo -e "  • Set ENABLE_REBOOT_ON_LOGOUT=false to disable"
echo -e "  • Restart your Node.js app for changes to take effect"
echo
echo -e "${YELLOW}⚠️  Warning: This will reboot the entire system on logout!${NC}"
echo -e "${YELLOW}   Make sure this is what you want.${NC}"