const express = require('express');
const session = require('express-session');
const bcrypt = require('bcryptjs');
const helmet = require('helmet');
const compression = require('compression');
const rateLimit = require('express-rate-limit');
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
    secure: process.env.NODE_ENV === 'production',
    httpOnly: true,
    maxAge: 24 * 60 * 60 * 1000 // 24 hours
  }
}));

// Security headers with CSP for Three.js
app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
      styleSrc: ["'self'", "'unsafe-inline'", "https://fonts.googleapis.com"],
      fontSrc: ["'self'", "https://fonts.gstatic.com"],
      connectSrc: ["'self'", "ws:", "wss:", `http://${TERMINAL_HOST}:${TERMINAL_PORT}`],
      frameSrc: ["'self'", `http://${TERMINAL_HOST}:${TERMINAL_PORT}`],
      imgSrc: ["'self'", "data:", "blob:"],
      workerSrc: ["'self'", "blob:"],
    },
  },
  crossOriginEmbedderPolicy: false,
}));

// Authentication middleware
const requireAuth = (req, res, next) => {
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
      console.log('Login successful');
      res.json({ success: true });
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
  res.json({
    host: TERMINAL_HOST,
    port: TERMINAL_PORT,
    url: `http://${TERMINAL_HOST}:${TERMINAL_PORT}`
  });
});

app.get('/', requireAuth, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

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