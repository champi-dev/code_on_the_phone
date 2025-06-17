const express = require('express');
const session = require('express-session');
const FileStore = require('session-file-store')(session);
const bcrypt = require('bcryptjs');
const helmet = require('helmet');
const compression = require('compression');
const rateLimit = require('express-rate-limit');
const { createProxyMiddleware } = require('http-proxy-middleware');
const path = require('path');
const http = require('http');
const fs = require('fs');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// Password hash - set this in Render environment variables
const PASSWORD_HASH = process.env.PASSWORD_HASH || bcrypt.hashSync('cloudterm123', 10);
const TERMINAL_HOST = process.env.TERMINAL_HOST || '142.93.249.123';
const TERMINAL_PORT = process.env.TERMINAL_PORT || '7681';

// Rate limiting
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 100 // limit each IP to 100 requests per windowMs
});

// Middleware
app.use(limiter);
app.use(compression());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

// Create sessions directory if it doesn't exist
const sessionsDir = path.join(__dirname, 'sessions');
if (!fs.existsSync(sessionsDir)) {
  fs.mkdirSync(sessionsDir, { recursive: true });
}

// Session middleware configuration with file store
const sessionConfig = {
  store: new FileStore({
    path: sessionsDir,
    ttl: 30 * 24 * 60 * 60, // 30 days
    retries: 5,
    reapInterval: 60 * 60, // Clean up expired sessions every hour
    logFn: function(){} // Disable logging
  }),
  secret: process.env.SESSION_SECRET || 'cloud-terminal-secret-2025',
  resave: false,
  saveUninitialized: false,
  cookie: {
    secure: false, // Set to false for now to work with HTTP
    httpOnly: true,
    maxAge: 30 * 24 * 60 * 60 * 1000, // 30 days - keep logged in for a month
    sameSite: 'lax'
  },
  name: 'sessionId',
  proxy: true // Trust the reverse proxy
};

const sessionMiddleware = session(sessionConfig);
app.use(sessionMiddleware);

// Security headers with CSP for Three.js
app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
      styleSrc: ["'self'", "'unsafe-inline'", "https://fonts.googleapis.com"],
      fontSrc: ["'self'", "https://fonts.gstatic.com"],
      connectSrc: ["'self'", "ws:", "wss:"],
      frameSrc: ["'self'"],
      imgSrc: ["'self'", "data:", "blob:"],
      workerSrc: ["'self'", "blob:"],
    },
  },
  crossOriginEmbedderPolicy: false,
}));

// Authentication middleware
const requireAuth = (req, res, next) => {
  console.log('Auth check:', {
    path: req.path,
    sessionId: req.sessionID,
    authenticated: req.session.authenticated,
    session: req.session
  });
  
  if (req.session.authenticated) {
    // Update last activity time
    req.session.lastActivity = new Date().toISOString();
    next();
  } else {
    // Check if this is an API request
    if (req.path.startsWith('/api/')) {
      res.status(401).json({ error: 'Unauthorized', redirect: '/login' });
    } else {
      res.redirect('/login');
    }
  }
};

// Routes
app.get('/login', (req, res) => {
  // If already authenticated, redirect to main page
  if (req.session.authenticated) {
    res.redirect('/');
  } else {
    res.sendFile(path.join(__dirname, 'public', 'login.html'));
  }
});

app.post('/api/login', async (req, res) => {
  const { password } = req.body;
  
  console.log('Login attempt:', {
    passwordReceived: !!password,
    passwordLength: password ? password.length : 0,
    passwordHashSet: !!PASSWORD_HASH
  });
  
  try {
    if (password && await bcrypt.compare(password, PASSWORD_HASH)) {
      req.session.authenticated = true;
      req.session.loginTime = new Date().toISOString();
      req.session.lastActivity = new Date().toISOString();
      req.session.save((err) => {
        if (err) {
          console.error('Session save error:', err);
          res.status(500).json({ success: false, message: 'Session error' });
        } else {
          console.log('Login successful, session saved');
          res.json({ 
            success: true,
            sessionInfo: {
              expiresIn: '30 days',
              persistent: true
            }
          });
        }
      });
    } else {
      console.log('Login failed - password mismatch');
      res.status(401).json({ success: false, message: 'Invalid password' });
    }
  } catch (error) {
    console.error('Login error:', error);
    res.status(500).json({ success: false, message: 'Server error' });
  }
});

app.post('/api/logout', (req, res) => {
  req.session.destroy((err) => {
    if (err) {
      console.error('Error destroying session:', err);
    }
    res.clearCookie('sessionId');
    res.json({ success: true });
    
    const { exec } = require('child_process');
    
    // Kill all user processes before reboot/logout
    if (process.env.ENABLE_PROCESS_CLEANUP === 'true' || process.env.ENABLE_REBOOT_ON_LOGOUT === 'true') {
      console.log('Logout detected - cleaning up user processes...');
      
      // Kill all processes except essential system processes
      const cleanupCommands = [
        // Kill all node processes except our own
        "pkill -f 'node(?!.*server\\.js)'",
        // Kill any Python processes
        "pkill -f python",
        // Kill any user shells (except init)
        "pkill -f '(bash|zsh|sh)' || true",
        // Kill any terminal multiplexers
        "pkill -f '(tmux|screen)' || true",
        // Kill any editors
        "pkill -f '(vim|nvim|nano|emacs)' || true",
        // Kill any development servers
        "pkill -f '(webpack|vite|next|react)' || true",
        // Clean up any zombie processes
        "ps aux | grep -E 'Z|<defunct>' | awk '{print $2}' | xargs -r kill -9 2>/dev/null || true",
        // Clear system cache
        "sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true"
      ];
      
      // Execute cleanup commands
      cleanupCommands.forEach(cmd => {
        exec(cmd, (error, stdout, stderr) => {
          if (error && !error.message.includes('No such process')) {
            console.log(`Cleanup command: ${cmd} - ${error.message}`);
          }
        });
      });
    }
    
    // Trigger system reboot if enabled
    if (process.env.ENABLE_REBOOT_ON_LOGOUT === 'true') {
      console.log('Initiating system reboot...');
      
      // Give the response and cleanup time before rebooting
      setTimeout(() => {
        // Try different reboot commands for compatibility
        exec('sudo reboot', (error) => {
          if (error) {
            // Try without sudo (if running as root)
            exec('reboot', (error2) => {
              if (error2) {
                // Try systemctl
                exec('systemctl reboot', (error3) => {
                  if (error3) {
                    console.error('Failed to reboot system:', error3);
                  }
                });
              }
            });
          }
        });
      }, 2000); // Increased timeout to allow cleanup
    }
  });
});

app.get('/api/terminal-config', requireAuth, (req, res) => {
  // Use proxy endpoint for HTTPS sites
  res.json({
    host: TERMINAL_HOST,
    port: TERMINAL_PORT,
    url: '/terminal-proxy',
    // Add a flag to indicate terminal might be offline
    checkHealth: true,
    // Include reboot on logout status
    rebootOnLogout: process.env.ENABLE_REBOOT_ON_LOGOUT === 'true'
  });
});

app.get('/', requireAuth, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Terminal health check
app.get('/api/terminal-health', requireAuth, async (req, res) => {
  const http = require('http');
  
  const checkTerminal = new Promise((resolve) => {
    const options = {
      hostname: TERMINAL_HOST,
      port: TERMINAL_PORT,
      path: '/',
      method: 'GET',
      timeout: 5000,
      headers: {
        'User-Agent': 'CloudTerminal/1.0'
      }
    };
    
    console.log('Health check to:', `http://${TERMINAL_HOST}:${TERMINAL_PORT}/`);
    
    const req = http.request(options, (response) => {
      console.log('Health check response:', response.statusCode);
      resolve({ available: response.statusCode < 500 });
    });
    
    req.on('error', (err) => {
      console.error('Health check error:', err.message);
      resolve({ available: false });
    });
    req.on('timeout', () => {
      console.error('Health check timeout');
      req.destroy();
      resolve({ available: false });
    });
    
    req.end();
  });
  
  const result = await checkTerminal;
  res.json(result);
});

// Handle fallback requests directly
app.get('/terminal-proxy', requireAuth, (req, res, next) => {
  if (req.query.fallback === 'true') {
    // Return fallback UI directly
    res.send(`
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body {
            margin: 0;
            background: #0d1117;
            color: #c9d1d9;
            font-family: monospace;
            padding: 20px;
            height: 100vh;
            box-sizing: border-box;
          }
          .terminal-info {
            background: rgba(122, 162, 247, 0.1);
            border: 1px solid rgba(122, 162, 247, 0.3);
            border-radius: 6px;
            padding: 20px;
            margin-bottom: 20px;
          }
          .terminal-demo {
            background: #161b22;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 20px;
            height: calc(100% - 120px);
            overflow: auto;
          }
          .prompt {
            color: #7ee787;
          }
          pre {
            margin: 0;
            white-space: pre-wrap;
          }
        </style>
      </head>
      <body>
        <div class="terminal-info">
          ℹ️ Terminal server is currently offline. This is a demo interface.
        </div>
        <div class="terminal-demo">
          <pre><span class="prompt">cloud@terminal:~$</span> echo "Welcome to Cloud Terminal 3D!"
Welcome to Cloud Terminal 3D!

<span class="prompt">cloud@terminal:~$</span> date
${new Date().toString()}

<span class="prompt">cloud@terminal:~$</span> uname -a
Linux cloud-terminal 5.15.0 #1 SMP x86_64 GNU/Linux

<span class="prompt">cloud@terminal:~$</span> ls -la
total 48
drwxr-xr-x 6 cloud cloud 4096 Jun 16 00:00 .
drwxr-xr-x 3 root  root  4096 Jun 16 00:00 ..
-rw-r--r-- 1 cloud cloud  220 Jun 16 00:00 .bash_logout
-rw-r--r-- 1 cloud cloud 3771 Jun 16 00:00 .bashrc
drwxr-xr-x 2 cloud cloud 4096 Jun 16 00:00 .config
drwxr-xr-x 3 cloud cloud 4096 Jun 16 00:00 .local
-rw-r--r-- 1 cloud cloud  807 Jun 16 00:00 .profile
drwxr-xr-x 2 cloud cloud 4096 Jun 16 00:00 projects

<span class="prompt">cloud@terminal:~$</span> # To connect a real terminal:
<span class="prompt">cloud@terminal:~$</span> # 1. Set up ttyd server
<span class="prompt">cloud@terminal:~$</span> # 2. Update TERMINAL_HOST in Render

<span class="prompt">cloud@terminal:~$</span> _</pre>
        </div>
      </body>
      </html>
    `);
    return;
  }
  
  // Otherwise, continue to next middleware
  next();
});

// Terminal proxy middleware
const terminalProxy = createProxyMiddleware({
  target: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`,
  ws: true,
  changeOrigin: true,
  pathRewrite: {
    '^/terminal-proxy': ''
  },
  timeout: 30000, // 30 second timeout
  proxyTimeout: 30000,
  logLevel: 'debug',
  // Handle WebSocket upgrade
  onProxyReqWs: (proxyReq, req, socket, options, head) => {
    console.log('WebSocket upgrade request');
    // Add authentication headers if needed
    proxyReq.setHeader('X-Forwarded-For', req.socket.remoteAddress);
  },
  onProxyReq: (proxyReq, req, res) => {
    console.log('Proxy request to:', `http://${TERMINAL_HOST}:${TERMINAL_PORT}${req.url}`);
    // Ensure proper headers for ttyd
    proxyReq.setHeader('Host', `${TERMINAL_HOST}:${TERMINAL_PORT}`);
  },
  onProxyRes: (proxyRes, req, res) => {
    console.log('Proxy response status:', proxyRes.statusCode);
  },
  onError: (err, req, res) => {
    console.error('Proxy error:', err.message);
    console.error('Error code:', err.code);
    console.error('Terminal host:', TERMINAL_HOST, 'Port:', TERMINAL_PORT);
    
    // Send a fallback HTML page instead of error
    res.status(200).send(`
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body {
            margin: 0;
            background: #0d1117;
            color: #c9d1d9;
            font-family: monospace;
            padding: 20px;
            height: 100vh;
            box-sizing: border-box;
          }
          .terminal-error {
            background: rgba(248, 81, 73, 0.1);
            border: 1px solid rgba(248, 81, 73, 0.3);
            border-radius: 6px;
            padding: 20px;
            margin-bottom: 20px;
          }
          .terminal-demo {
            background: #161b22;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 20px;
            height: calc(100% - 120px);
            overflow: auto;
          }
          .prompt {
            color: #7ee787;
          }
          pre {
            margin: 0;
            white-space: pre-wrap;
          }
        </style>
      </head>
      <body>
        <div class="terminal-error">
          ⚠️ Terminal server unavailable. The external terminal service at ${TERMINAL_HOST}:${TERMINAL_PORT} is not responding.
        </div>
        <div class="terminal-demo">
          <pre><span class="prompt">$</span> # Terminal Demo Mode
<span class="prompt">$</span> echo "Welcome to Cloud Terminal 3D!"
Welcome to Cloud Terminal 3D!

<span class="prompt">$</span> # The real terminal server is currently offline
<span class="prompt">$</span> # This is a demo interface showing what it would look like

<span class="prompt">$</span> ls -la
total 64
drwxr-xr-x   5 user  staff   160 Jun 16 00:00 .
drwxr-xr-x  10 user  staff   320 Jun 16 00:00 ..
-rw-r--r--   1 user  staff  1234 Jun 16 00:00 app.js
-rw-r--r--   1 user  staff  2048 Jun 16 00:00 package.json
drwxr-xr-x   3 user  staff    96 Jun 16 00:00 node_modules

<span class="prompt">$</span> # To set up your own terminal server:
<span class="prompt">$</span> # 1. Install ttyd: https://github.com/tsl0922/ttyd
<span class="prompt">$</span> # 2. Run: ttyd -p 7681 bash
<span class="prompt">$</span> # 3. Update TERMINAL_HOST in Render environment variables

<span class="prompt">$</span> _</pre>
        </div>
      </body>
      </html>
    `);
  }
});

// Apply terminal proxy to route
app.use('/terminal-proxy', requireAuth, terminalProxy);

// Service worker and manifest
app.get('/sw.js', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'sw.js'));
});

app.get('/manifest.json', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'manifest.json'));
});

app.get('/favicon.ico', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'favicon.svg'));
});

// Test proxy configuration
app.get('/api/proxy-test', requireAuth, async (req, res) => {
  const http = require('http');
  
  const testProxy = new Promise((resolve) => {
    const postData = '';
    const options = {
      hostname: TERMINAL_HOST,
      port: TERMINAL_PORT,
      path: '/',
      method: 'GET',
      headers: {
        'Content-Type': 'text/html',
        'Content-Length': Buffer.byteLength(postData)
      }
    };
    
    console.log('Direct test to:', `http://${TERMINAL_HOST}:${TERMINAL_PORT}/`);
    
    const req = http.request(options, (response) => {
      let data = '';
      response.on('data', (chunk) => {
        data += chunk;
      });
      response.on('end', () => {
        resolve({
          statusCode: response.statusCode,
          headers: response.headers,
          bodyLength: data.length,
          bodyPreview: data.substring(0, 200)
        });
      });
    });
    
    req.on('error', (err) => {
      resolve({
        error: err.message,
        code: err.code
      });
    });
    
    req.write(postData);
    req.end();
  });
  
  const result = await testProxy;
  res.json({
    terminalHost: TERMINAL_HOST,
    terminalPort: TERMINAL_PORT,
    proxyTarget: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`,
    testResult: result
  });
});

// Debug terminal input
app.get('/api/terminal-debug', requireAuth, (req, res) => {
  res.json({
    terminalUrl: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`,
    proxyUrl: '/terminal-proxy',
    wsUrl: `ws://${TERMINAL_HOST}:${TERMINAL_PORT}/ws`,
    instructions: [
      'Terminal is loaded via iframe proxy',
      'WebSocket connection needed for input',
      'Check browser console for errors',
      'Ensure ttyd is running with: ttyd -W -i 0.0.0.0 -p 7681 bash'
    ]
  });
});

// Session status endpoint
app.get('/api/session-status', requireAuth, (req, res) => {
  res.json({
    authenticated: true,
    loginTime: req.session.loginTime,
    lastActivity: req.session.lastActivity,
    sessionExpiry: new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString()
  });
});

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Create HTTP server for WebSocket support
const server = http.createServer(app);


// Handle WebSocket upgrade for terminal
server.on('upgrade', (request, socket, head) => {
  console.log('WebSocket upgrade request for:', request.url);
  
  if (request.url.startsWith('/terminal-proxy')) {
    // Parse cookies and check session
    const cookies = request.headers.cookie || '';
    const sessionId = cookies.split(';').find(c => c.trim().startsWith('sessionId='));
    
    if (sessionId) {
      console.log('WebSocket has session cookie, allowing upgrade');
      terminalProxy.upgrade(request, socket, head);
    } else {
      console.log('WebSocket missing auth, rejecting');
      socket.write('HTTP/1.1 401 Unauthorized\r\n\r\n');
      socket.destroy();
    }
  }
});

// Start server
server.listen(PORT, () => {
  console.log(`Cloud Terminal 3D running on port ${PORT}`);
  console.log(`Terminal backend: ${TERMINAL_HOST}:${TERMINAL_PORT}`);
});