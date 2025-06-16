# Cloud Terminal - Render Frontend

Deploy this to Render.com for a beautiful web interface to your cloud terminal.

## Quick Deploy

1. **Set up your VPS first**:
   ```bash
   ssh root@your-vps-ip
   curl -sSL https://raw.githubusercontent.com/your-repo/scripts/vps/quick-setup.sh | bash
   ```

2. **Deploy to Render**:
   - Push this folder to GitHub
   - Connect GitHub to Render
   - Create new Web Service
   - Set environment variables:
     - `TERMINAL_HOST`: Your VPS IP
     - `TERMINAL_PORT`: 7681

3. **Access your terminal**:
   - `https://your-app.onrender.com`

## Features

- Mobile-friendly responsive design
- Quick command buttons
- O(1) performance indicators
- Beautiful Tokyo Night theme
- No domain purchase needed!

## Security Notes

For production use, consider:
- Setting up HTTPS on your VPS
- Using authentication on ttyd
- Restricting IP access