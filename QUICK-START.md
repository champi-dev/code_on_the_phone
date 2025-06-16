# 🚀 Quick Start Guide

## Option 1: Instant (0 Setup) - Google Cloud Shell

```bash
# Just click this link:
https://shell.cloud.google.com
```

That's it! You get a free terminal with Python, Node.js, and 5GB storage.

## Option 2: VPS + Web Terminal (5 minutes)

### Step 1: Get a VPS

**DigitalOcean** (Recommended - $200 free credit):
1. Sign up: https://digitalocean.com
2. Create Droplet: Ubuntu 24.04, $12/month
3. Note your server IP

**Oracle Cloud** (FREE forever):
1. Sign up: https://oracle.com/cloud/free/
2. Create VM: Ubuntu, ARM (4 CPU, 24GB RAM!)
3. Note your server IP

### Step 2: Install Terminal

SSH into your VPS and run ONE command:

```bash
# Quick setup (3 minutes):
curl -sSL https://bit.ly/cloud-terminal-quick | bash

# OR full setup with all libraries (10 minutes):
curl -sSL https://bit.ly/cloud-terminal-full | bash
```

### Step 3: Access Your Terminal

Open in browser: `http://YOUR-SERVER-IP:7681`

## Option 3: With Render Frontend (HTTPS)

1. Complete Option 2 above
2. Deploy `scripts/frontend/render-terminal/` to Render.com
3. Set environment variable: `TERMINAL_HOST=your-vps-ip`
4. Access at: `https://your-app.onrender.com`

## What You Get

- **Beautiful Terminal**: Zsh + Oh My Zsh + Starship prompt
- **Dev Tools**: Python, Node.js, Go, Rust, Docker
- **ML/Science**: NumPy, PyTorch, TensorFlow, Jupyter
- **Mobile Friendly**: Works great on phones/tablets
- **Persistent**: tmux sessions survive disconnects
- **Fast**: O(1) operations with caching

## Commands to Try

```bash
# Python with all scientific libraries
python3
>>> import numpy as np
>>> import torch
>>> import tensorflow as tf

# Jupyter notebook
jupyter notebook --ip=0.0.0.0 --no-browser

# Beautiful system monitor
btop

# Start new tmux window
Ctrl+B, C
```

## Total Time: 5 minutes
## Total Cost: $0-12/month

Questions? Check the [README](README.md) for more options!