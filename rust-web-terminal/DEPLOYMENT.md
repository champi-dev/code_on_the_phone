# Deployment Guide

## Production Deployment

### Option 1: GitHub Pages
1. Push to main branch
2. CI/CD automatically builds and deploys to GitHub Pages
3. Access at: `https://[username].github.io/rust-web-terminal/`

### Option 2: Vercel
```bash
# Install Vercel CLI
npm i -g vercel

# Build the project
./build.sh

# Deploy
vercel --prod
```

### Option 3: Netlify
1. Connect GitHub repository
2. Build command: `./build.sh`
3. Publish directory: `./`
4. Deploy

### Option 4: Self-Hosted
```bash
# Build for production
wasm-pack build --target web --out-dir pkg --release

# Serve with nginx
server {
    listen 80;
    server_name your-domain.com;
    
    location / {
        root /var/www/rust-web-terminal;
        try_files $uri $uri/ /index.html;
        
        # CORS headers for WASM
        add_header Cross-Origin-Embedder-Policy "require-corp";
        add_header Cross-Origin-Opener-Policy "same-origin";
    }
    
    # WASM mime type
    location ~ \.wasm$ {
        add_header Content-Type application/wasm;
    }
}
```

## Security Considerations

1. **HTTPS Required**: Always use HTTPS in production
2. **CSP Headers**: Configure Content Security Policy
3. **Authentication**: Consider adding additional auth layers
4. **Rate Limiting**: Implement connection rate limits

## Environment Variables

Create `.env.production`:
```
TERMINAL_HOST=142.93.249.123
TERMINAL_PORT=7681
WSS_PROTOCOL=wss
```

## Monitoring

### CloudWatch/Datadog Integration
```javascript
// Add to index.html
window.addEventListener('error', (e) => {
    // Log to monitoring service
    console.error('Terminal Error:', e);
});
```

## Performance Optimization

1. **Enable Brotli compression**
2. **Use CDN for static assets**
3. **Implement service worker for offline support**
4. **Enable HTTP/2**

## Troubleshooting

### WASM Loading Issues
- Check CORS headers
- Verify WASM mime type
- Check browser console for errors

### WebSocket Connection Failed
- Verify droplet is accessible
- Check firewall rules
- Confirm ttyd is running

### Performance Issues
- Check browser dev tools performance tab
- Monitor WebSocket message frequency
- Verify no memory leaks