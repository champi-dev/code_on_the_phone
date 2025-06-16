const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');
const helmet = require('helmet');
const compression = require('compression');
const path = require('path');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// Configuration from environment variables
const TERMINAL_HOST = process.env.TERMINAL_HOST;
const TERMINAL_PORT = process.env.TERMINAL_PORT || '7681';

if (!TERMINAL_HOST) {
  console.error('ERROR: TERMINAL_HOST environment variable is required');
  process.exit(1);
}

// Security middleware
app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
      styleSrc: ["'self'", "'unsafe-inline'"],
      connectSrc: ["'self'", "ws:", "wss:", `http://${TERMINAL_HOST}:${TERMINAL_PORT}`],
      frameSrc: ["'self'", `http://${TERMINAL_HOST}:${TERMINAL_PORT}`],
    },
  },
  crossOriginEmbedderPolicy: false,
}));

app.use(compression());
app.use(express.static(path.join(__dirname, 'public')));

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Main route
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

const server = app.listen(PORT, () => {
  console.log(`Cloud Terminal running on port ${PORT}`);
  console.log(`Proxying to ${TERMINAL_HOST}:${TERMINAL_PORT}`);
});