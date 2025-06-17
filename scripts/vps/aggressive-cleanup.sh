#!/bin/bash
# Aggressive system cleanup script for cloud terminals
# Kills all non-essential processes and frees up resources

set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🧹 Aggressive System Cleanup${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}This script must be run as root or with sudo${NC}" 
   exit 1
fi

echo -e "${GREEN}📊 System status before cleanup:${NC}"
echo "Memory usage:"
free -h
echo
echo "Top processes:"
ps aux --sort=-%mem | head -10

echo -e "\n${YELLOW}🔄 Starting aggressive cleanup...${NC}"

# 1. Kill all user processes except system essentials
echo -e "${GREEN}1️⃣ Killing non-essential processes...${NC}"

# Get list of essential processes to preserve
ESSENTIAL_PIDS=$(ps aux | grep -E '(systemd|init|kernel|ssh|ttyd|node.*server\.js)' | awk '{print $2}' | tr '\n' '|' | sed 's/|$//')

# Kill everything else
if [ -n "$ESSENTIAL_PIDS" ]; then
    ps aux | awk '{print $2}' | grep -vE "^($ESSENTIAL_PIDS)$" | grep -v "^PID$" | while read pid; do
        if [ -n "$pid" ] && [ "$pid" != "$$" ]; then
            kill -9 "$pid" 2>/dev/null || true
        fi
    done
fi

# 2. Clean up specific process types
echo -e "${GREEN}2️⃣ Cleaning up specific processes...${NC}"

# Development processes
pkill -9 -f 'webpack|vite|next|react|vue|angular|svelte' 2>/dev/null || true
pkill -9 -f 'npm|yarn|pnpm|pip|cargo|go' 2>/dev/null || true
pkill -9 -f 'python|ruby|perl|java|dotnet' 2>/dev/null || true

# Editors and IDEs
pkill -9 -f 'code|vim|nvim|emacs|nano|micro' 2>/dev/null || true

# Shells and terminals (except current)
pkill -9 -f 'bash|zsh|fish' 2>/dev/null || true

# Clean up tmux and screen sessions
tmux kill-server 2>/dev/null || true
screen -ls | grep Detached | cut -d. -f1 | awk '{print $1}' | xargs -r kill 2>/dev/null || true

# 3. Memory cleanup
echo -e "${GREEN}3️⃣ Freeing up memory...${NC}"

# Drop caches
sync
echo 1 > /proc/sys/vm/drop_caches
echo 2 > /proc/sys/vm/drop_caches
echo 3 > /proc/sys/vm/drop_caches

# Clear swap if used
swapoff -a 2>/dev/null && swapon -a 2>/dev/null || true

# 4. Clean up disk space
echo -e "${GREEN}4️⃣ Cleaning up disk space...${NC}"

# Clean package manager cache
apt-get clean 2>/dev/null || true
yum clean all 2>/dev/null || true
dnf clean all 2>/dev/null || true

# Clean npm cache
npm cache clean --force 2>/dev/null || true

# Clean pip cache
pip cache purge 2>/dev/null || true

# Remove old logs
find /var/log -type f -name "*.log" -mtime +7 -delete 2>/dev/null || true
journalctl --vacuum-time=7d 2>/dev/null || true

# Clean tmp directories
find /tmp -type f -atime +7 -delete 2>/dev/null || true
find /var/tmp -type f -atime +7 -delete 2>/dev/null || true

# 5. Network cleanup
echo -e "${GREEN}5️⃣ Cleaning up network connections...${NC}"

# Close all non-SSH connections
ss -tulpn | grep -v ':22\|:7681' | awk '{print $5}' | grep -oE '[0-9]+$' | sort -u | while read port; do
    fuser -k "$port"/tcp 2>/dev/null || true
done

# 6. Final cleanup
echo -e "${GREEN}6️⃣ Final cleanup...${NC}"

# Kill any remaining zombie processes
ps aux | grep -E 'Z|<defunct>' | awk '{print $2}' | xargs -r kill -9 2>/dev/null || true

# Restart essential services
systemctl restart systemd-resolved 2>/dev/null || true
systemctl restart systemd-networkd 2>/dev/null || true

echo -e "\n${GREEN}📊 System status after cleanup:${NC}"
echo "Memory usage:"
free -h
echo
echo "Running processes:"
ps aux | wc -l

echo -e "\n${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✅ Aggressive cleanup completed!${NC}"
echo -e "${BLUE}   Memory and resources have been freed${NC}"
echo -e "${BLUE}   System is ready for new sessions${NC}"