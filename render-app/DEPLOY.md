# 🚀 Deploy Terminal 3D to Render

## One-Click Deploy

1. **Fork this repository** on GitHub
2. **Go to [render.com](https://render.com)** and sign in
3. Click **"New +"** → **"Web Service"**
4. **Connect your GitHub** account if not already connected
5. **Select your forked repository**
6. Render will automatically detect the `render.yaml` file
7. Click **"Create Web Service"**

That's it! Your terminal will be live in ~3 minutes at:
`https://[your-service-name].onrender.com`

## Default Credentials

- **Password**: `terminal123`
- **Username**: Not required (password-only auth)

## Features Deployed

✅ **Award-winning 3D Animations**:
- Galaxy system (50k particles) - triggered by `git` commands
- Quantum field with GLSL shaders - triggered by `npm`/`node`
- Holographic displays - triggered by `ssh`/`connect`
- Neural network visualization - triggered by `python`/`ai`
- Matrix rain effect - triggered by `sudo`/`hack`

✅ **Terminal Features**:
- Real command execution with `exec <command>`
- Tab management
- Command history
- Mobile-optimized with quick buttons
- PWA installable

✅ **Performance Optimizations**:
- Linux/low-end device detection
- Reduced particles and FPS on slower systems
- Optimized WebGL rendering
- Service worker caching

## Customization

### Change Password

1. Generate new password hash locally:
```bash
node -e "console.log(require('bcryptjs').hashSync('your-new-password', 10))"
```

2. In Render dashboard:
   - Go to Environment
   - Update `PASSWORD_HASH` with the new hash
   - Save changes

### Environment Variables

All configured in `render.yaml`:
- `NODE_ENV`: production
- `TERMINAL_HOST`: Backend terminal server
- `TERMINAL_PORT`: Terminal port
- `SESSION_SECRET`: Auto-generated secure key
- `PASSWORD_HASH`: Bcrypt hash of password
- `CHECK_TERMINAL_HEALTH`: false (local terminal)
- `ENABLE_REBOOT_ON_LOGOUT`: false

### Custom Domain

1. In Render dashboard → Settings
2. Add your custom domain
3. Follow DNS configuration instructions

## Architecture

```
┌─────────────────┐
│   Browser PWA   │
├─────────────────┤
│  Three.js 3D    │
│  Animations     │
├─────────────────┤
│  Express Server │
│  - Auth         │
│  - Sessions     │
│  - API Routes   │
├─────────────────┤
│  Terminal       │
│  - exec endpoint│
│  - WebSocket    │
└─────────────────┘
```

## Monitoring

- Health check endpoint: `/login`
- Logs: Available in Render dashboard
- Metrics: CPU, Memory, Response times

## Troubleshooting

**Terminal not working?**
- The terminal uses local command execution
- Commands run with `exec` prefix
- 5-second timeout, 1MB output limit

**3D animations laggy?**
- Automatically reduces quality on Linux/mobile
- Try refreshing the page
- Install as PWA for better performance

**Can't login?**
- Default password: `terminal123`
- Check PASSWORD_HASH environment variable
- Clear browser cookies and try again

## Security Notes

- Change default password immediately
- Use strong SESSION_SECRET
- Rate limited to 100 requests/15min
- Dangerous commands blocked (rm -rf, dd, etc.)
- HTTPS enforced by Render

## Updates

When you push to main branch:
- Render auto-deploys within minutes
- Zero downtime deployments
- Automatic rollback on failure

---

Enjoy your cloud terminal with award-winning 3D animations! 🎉