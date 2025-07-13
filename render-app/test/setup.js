// Global test setup
const { TextEncoder, TextDecoder } = require('util');

// Add TextEncoder/TextDecoder to global for JSDOM
global.TextEncoder = TextEncoder;
global.TextDecoder = TextDecoder;

// Mock console methods to reduce noise during tests
const originalConsole = { ...console };
global.console = {
  ...console,
  log: jest.fn(),
  error: jest.fn(),
  warn: jest.fn(),
  info: jest.fn(),
  debug: jest.fn()
};

// Restore console for debugging when needed
global.restoreConsole = () => {
  global.console = originalConsole;
};

// Setup fetch mock
global.fetch = jest.fn();

// Setup WebSocket mock
global.WebSocket = jest.fn(() => ({
  CONNECTING: 0,
  OPEN: 1,
  CLOSING: 2,
  CLOSED: 3,
  addEventListener: jest.fn(),
  removeEventListener: jest.fn(),
  send: jest.fn(),
  close: jest.fn(),
  readyState: 1
}));

// Static properties
global.WebSocket.CONNECTING = 0;
global.WebSocket.OPEN = 1;
global.WebSocket.CLOSING = 2;
global.WebSocket.CLOSED = 3;

// Setup performance mock
global.performance = {
  now: jest.fn(() => Date.now())
};

// Clean up after each test
afterEach(() => {
  jest.clearAllMocks();
});

// Increase test timeout for CI environments
if (process.env.CI) {
  jest.setTimeout(30000);
}