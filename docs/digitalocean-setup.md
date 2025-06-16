# DigitalOcean Setup Guide - Cloud Terminal

## Step 1: Create Your Droplet

1. From the DigitalOcean dashboard, click **"Deploy a virtual machine"**

2. **Choose an image**:
   - Select **Ubuntu 24.04 (LTS) x64**

3. **Choose a plan**:
   - **Basic** → **Regular**
   - Select **$12/month** (2 vCPUs, 2GB RAM, 50GB SSD)
   - Or **$18/month** (2 vCPUs, 4GB RAM, 80GB SSD) for better performance

4. **Choose a datacenter**:
   - Pick closest to you (e.g., New York, San Francisco, London)

5. **Authentication**:
   - **SSH Keys** (recommended):
     - Click "New SSH Key"
     - Run this on your computer: `cat ~/.ssh/id_rsa.pub`
     - Copy & paste the output
   - **OR Password** (easier but less secure)

6. **Finalize**:
   - Hostname: `cloud-terminal`
   - Click **"Create Droplet"**

## Step 2: Connect to Your Droplet

Wait ~55 seconds for creation, then:

```bash
# Copy your Droplet's IP address from the dashboard
# Then connect:
ssh root@YOUR_DROPLET_IP
```

## Step 3: Install Cloud Terminal (One Command!)

Once connected, run:

```bash
# Quick install (3 minutes):
curl -sSL https://raw.githubusercontent.com/your-username/code_on_the_phone/main/scripts/vps/quick-setup.sh | bash

# OR full install with all libraries (10 minutes):
curl -sSL https://raw.githubusercontent.com/your-username/code_on_the_phone/main/scripts/vps/full-setup.sh | bash
```

## Step 4: Access Your Terminal

Open in your browser:
```
http://YOUR_DROPLET_IP:7681
```

That's it! You now have a beautiful cloud terminal accessible from anywhere.

## Optional: Enable Firewall (Recommended)

In DigitalOcean dashboard:
1. Go to Networking → Firewalls
2. Create Firewall
3. Add rules:
   - SSH (port 22) - your IP only
   - Custom (port 7681) - anywhere
4. Apply to your Droplet

## Quick Test Commands

```bash
# Beautiful prompt
zsh

# Python with libraries
python3
>>> import numpy as np
>>> import pandas as pd

# System monitor
btop

# Terminal multiplexer
tmux
```

## Tips

- Your terminal persists even if you disconnect
- Access from phone, tablet, or any browser
- All development tools pre-installed
- Total cost: $12-18/month

Need HTTPS? See the [Render deployment guide](../scripts/frontend/render-terminal/README.md)!