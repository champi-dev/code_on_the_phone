const express = require('express');
const session = require('express-session');
const bcrypt = require('bcryptjs');
const helmet = require('helmet');
const compression = require('compression');
const rateLimit = require('express-rate-limit');
const { createProxyMiddleware } = require('http-proxy-middleware');
const path = require('path');
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

// Session configuration
app.use(session({
  secret: process.env.SESSION_SECRET || 'cloud-terminal-secret-2025',
  resave: false,
  saveUninitialized: false,
  cookie: {
    secure: false, // Set to false for now to work with HTTP
    httpOnly: true,
    maxAge: 24 * 60 * 60 * 1000, // 24 hours
    sameSite: 'lax'
  },
  name: 'sessionId',
  proxy: true // Trust the reverse proxy
}));

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
  res.sendFile(path.join(__dirname, 'public', 'login.html'));
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
      req.session.save((err) => {
        if (err) {
          console.error('Session save error:', err);
          res.status(500).json({ success: false, message: 'Session error' });
        } else {
          console.log('Login successful, session saved');
          res.json({ success: true });
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
  req.session.destroy();
  res.json({ success: true });
});

app.get('/api/terminal-config', requireAuth, (req, res) => {
  // Use proxy endpoint for HTTPS sites
  res.json({
    host: TERMINAL_HOST,
    port: TERMINAL_PORT,
    url: '/terminal-proxy',
    // Add a flag to indicate terminal might be offline
    checkHealth: true
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
      timeout: 5000
    };
    
    const req = http.request(options, (response) => {
      resolve({ available: response.statusCode < 500 });
    });
    
    req.on('error', () => resolve({ available: false }));
    req.on('timeout', () => {
      req.destroy();
      resolve({ available: false });
    });
    
    req.end();
  });
  
  const result = await checkTerminal;
  res.json(result);
});

// Handle fallback requests directly
app.get('/terminal-proxy', requireAuth, (req, res) => {
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

// Terminal proxy with timeout handling
app.use('/terminal-proxy', requireAuth, createProxyMiddleware({
  target: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`,
  ws: true,
  changeOrigin: true,
  pathRewrite: {
    '^/terminal-proxy': ''
  },
  timeout: 10000, // 10 second timeout
  proxyTimeout: 10000,
  logLevel: 'debug',
  onProxyReq: (proxyReq, req, res) => {
    console.log('Proxy request to:', `http://${TERMINAL_HOST}:${TERMINAL_PORT}${req.url}`);
    console.log('Proxy headers:', proxyReq.getHeaders());
  },
  onProxyRes: (proxyRes, req, res) => {
    console.log('Proxy response status:', proxyRes.statusCode);
  },
  onError: (err, req, res) => {
    console.error('Proxy error:', err.message);
    console.error('Error code:', err.code);
    console.error('Terminal host:', TERMINAL_HOST, 'Port:', TERMINAL_PORT);
    console.error('Full error:', err);
    
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
  },
  onProxyReq: (proxyReq, req, res) => {
    console.log('Proxying to:', `http://${TERMINAL_HOST}:${TERMINAL_PORT}${req.url}`);
  }
}));

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

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

const server = app.listen(PORT, () => {
  console.log(`Cloud Terminal 3D running on port ${PORT}`);
  console.log(`Terminal backend: ${TERMINAL_HOST}:${TERMINAL_PORT}`);
});