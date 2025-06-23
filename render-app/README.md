# Terminal 3D - Render Deployment

Award-winning terminal with Three.js animations, PWA support, and password protection.

## Features

- 🎨 **Three.js Animations**: Stunning 120fps particle effects and geometric animations
- 🔒 **Password Protected**: Secure access with bcrypt authentication
- 📱 **PWA Ready**: Install as native app on any device
- ⚡ **O(1) Performance**: Optimized for speed with performance monitoring
- 🌐 **Responsive**: Beautiful on mobile, tablet, and desktop

## Quick Deploy to Render

1. **Push to GitHub**:
   ```bash
   git add render-app/
   git commit -m "Add Terminal 3D"
   git push
   ```

2. **Deploy on Render**:
   - Go to [render.com](https://render.com)
   - New → Web Service
   - Connect your GitHub repo
   - Set root directory: `render-app`
   - Build command: `npm install`
   - Start command: `npm start`

3. **Environment Variables** (in Render dashboard):
   ```
   TERMINAL_HOST=142.93.249.123
   TERMINAL_PORT=7681
   SESSION_SECRET=your-secret-key-here
   PASSWORD_HASH=$2a$10$xK1.BKDWYUQvtVl.W3Mjz.8rZKgX6IH5EYXL3jN8ifYJnL3GpXWlm
   ```

   Default password is `terminal123`. To change:
   ```bash
   node -e "console.log(require('bcryptjs').hashSync('your-new-password', 10))"
   ```

4. **Access your terminal**:
   - `https://your-app.onrender.com`
   - Enter password
   - Enjoy!

## Three.js Animations

The terminal features:
- Floating particle system with 1000+ particles
- Wireframe torus knot that responds to mouse movement
- Smooth 120fps performance with requestAnimationFrame
- GPU-accelerated WebGL rendering
- Real-time FPS counter

## PWA Features

- Installable on all devices
- Offline support with service worker
- App shortcuts for quick actions
- Standalone display mode
- Custom icons and splash screens

## Performance Optimizations

- Lazy loading of Three.js
- Efficient particle batching
- Frame limiting to preserve battery
- Backdrop filters with GPU acceleration
- Optimized WebSocket connections

## Security

- Session-based authentication
- Bcrypt password hashing
- Rate limiting (100 requests/15min)
- Secure headers with Helmet.js
- CSRF protection

## Customization

Edit `public/index.html` to:
- Adjust Three.js animations
- Change particle colors/counts
- Modify animation speeds
- Add new geometric shapes

Total deployment time: ~3 minutes! 🚀