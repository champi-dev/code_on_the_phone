const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');

// Mock self object for Web Worker environment
global.self = {
  addEventListener: jest.fn(),
  postMessage: jest.fn()
};

// Mock timers
global.setInterval = jest.fn();
global.clearInterval = jest.fn();
global.console = {
  log: jest.fn(),
  error: jest.fn()
};

describe('Connection Worker Tests', () => {
  let messageHandler;
  let intervalHandler;

  beforeEach(() => {
    jest.clearAllMocks();
    
    // Reset module cache to ensure clean state
    jest.resetModules();
    
    // Load the worker script
    require('../public/js/connection-worker.js');
    
    // Get the message event handler
    const messageCall = global.self.addEventListener.mock.calls.find(
      call => call[0] === 'message'
    );
    messageHandler = messageCall ? messageCall[1] : null;
    
    // Get the interval handler for health checks
    const intervalCall = global.setInterval.mock.calls.find(
      call => call[1] === 60000
    );
    intervalHandler = intervalCall ? intervalCall[0] : null;
  });

  describe('Connection Registration', () => {
    it('should register a new connection', () => {
      if (!messageHandler) {
        console.warn('Message handler not found, skipping test');
        return;
      }
      
      const tabConfig = {
        tabId: 'tab1',
        config: { url: 'http://localhost/terminal' }
      };

      messageHandler({
        data: {
          type: 'register',
          data: tabConfig
        }
      });

      expect(global.self.postMessage).toHaveBeenCalledWith({
        type: 'registered',
        tabId: 'tab1',
        timestamp: expect.any(Number)
      });
      
      expect(global.setInterval).toHaveBeenCalled();
    });

    it('should start keep-alive timer on registration', () => {
      messageHandler({
        data: {
          type: 'register',
          data: {
            tabId: 'tab1',
            config: {}
          }
        }
      });

      // Should create a timer for keep-alive
      const keepAliveCall = global.setInterval.mock.calls.find(
        call => call[1] === 30000
      );
      expect(keepAliveCall).toBeDefined();
    });
  });

  describe('Connection Unregistration', () => {
    it('should unregister a connection', () => {
      // First register
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Then unregister
      messageHandler({
        data: {
          type: 'unregister',
          data: { tabId: 'tab1' }
        }
      });

      expect(global.self.postMessage).toHaveBeenCalledWith({
        type: 'unregistered',
        tabId: 'tab1',
        timestamp: expect.any(Number)
      });
    });

    it('should clear keep-alive timer on unregistration', () => {
      // Mock timer ID
      const timerId = 123;
      global.setInterval.mockReturnValue(timerId);
      
      // Register first
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Clear mock calls to isolate the unregister action
      global.clearInterval.mockClear();

      // Unregister
      messageHandler({
        data: {
          type: 'unregister',
          data: { tabId: 'tab1' }
        }
      });

      expect(global.clearInterval).toHaveBeenCalledWith(timerId);
    });
  });

  describe('Keep-Alive Functionality', () => {
    it('should send keep-alive message', () => {
      // Register connection first
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Clear previous postMessage calls
      global.self.postMessage.mockClear();

      // Send keepalive
      messageHandler({
        data: {
          type: 'keepalive',
          data: { tabId: 'tab1' }
        }
      });

      expect(global.self.postMessage).toHaveBeenCalledWith({
        type: 'sendKeepAlive',
        tabId: 'tab1',
        timestamp: expect.any(Number)
      });
    });

    it('should not send keep-alive for non-existent connection', () => {
      global.self.postMessage.mockClear();

      messageHandler({
        data: {
          type: 'keepalive',
          data: { tabId: 'nonexistent' }
        }
      });

      expect(global.self.postMessage).not.toHaveBeenCalled();
    });

    it('should update keep-alive interval', () => {
      // Register a connection
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Update interval
      messageHandler({
        data: {
          type: 'updateInterval',
          data: { interval: 60000 }
        }
      });

      // Should restart timers with new interval
      const newTimerCall = global.setInterval.mock.calls.find(
        call => call[1] === 60000 && call[0].toString().includes('sendKeepAlive')
      );
      expect(newTimerCall).toBeDefined();
    });
  });

  describe('Status Reporting', () => {
    it('should report status of all connections', () => {
      // Register multiple connections
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab2', config: {} }
        }
      });

      // Clear previous calls
      global.self.postMessage.mockClear();

      // Request status
      messageHandler({
        data: { type: 'status' }
      });

      expect(global.self.postMessage).toHaveBeenCalledWith({
        type: 'status',
        data: {
          connections: expect.arrayContaining([
            expect.objectContaining({
              tabId: 'tab1',
              lastPing: expect.any(Number),
              active: true,
              uptime: expect.any(Number)
            }),
            expect.objectContaining({
              tabId: 'tab2',
              lastPing: expect.any(Number),
              active: true,
              uptime: expect.any(Number)
            })
          ]),
          keepAliveInterval: 30000
        },
        timestamp: expect.any(Number)
      });
    });
  });

  describe('Health Check', () => {
    it('should detect connections needing reconnect', () => {
      // Register a connection
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Mock current time to be 3 minutes later
      const originalDateNow = Date.now;
      Date.now = jest.fn(() => originalDateNow() + 180000);

      // Clear previous calls
      global.self.postMessage.mockClear();

      // Run health check
      if (intervalHandler) {
        intervalHandler();
      }

      expect(global.self.postMessage).toHaveBeenCalledWith({
        type: 'needsReconnect',
        tabId: 'tab1',
        lastPing: expect.any(Number),
        timestamp: expect.any(Number)
      });

      // Restore Date.now
      Date.now = originalDateNow;
    });

    it('should not trigger reconnect for active connections', () => {
      // Register and immediately update ping time
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      messageHandler({
        data: {
          type: 'keepalive',
          data: { tabId: 'tab1' }
        }
      });

      // Clear calls
      global.self.postMessage.mockClear();

      // Run health check
      if (intervalHandler) {
        intervalHandler();
      }

      // Should not send needsReconnect
      expect(global.self.postMessage).not.toHaveBeenCalledWith(
        expect.objectContaining({ type: 'needsReconnect' })
      );
    });
  });

  describe('Keep-Alive Timer Execution', () => {
    it('should execute keep-alive at intervals', () => {
      // Register connection
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Get the keep-alive timer function
      const keepAliveTimerCall = global.setInterval.mock.calls.find(
        call => call[1] === 30000
      );
      const keepAliveTimer = keepAliveTimerCall[0];

      // Clear previous calls
      global.self.postMessage.mockClear();

      // Execute the timer
      keepAliveTimer();

      expect(global.self.postMessage).toHaveBeenCalledWith({
        type: 'sendKeepAlive',
        tabId: 'tab1',
        timestamp: expect.any(Number)
      });
    });
  });

  describe('Edge Cases', () => {
    it('should handle unknown message types', () => {
      // Should not throw
      expect(() => {
        messageHandler({
          data: {
            type: 'unknown',
            data: {}
          }
        });
      }).not.toThrow();
    });

    it('should handle messages without data', () => {
      // Should not throw
      expect(() => {
        messageHandler({
          data: {
            type: 'register'
            // Missing data field
          }
        });
      }).not.toThrow();
    });

    it('should replace timer when re-registering same tab', () => {
      // Register once
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      const firstTimerCount = global.setInterval.mock.calls.length;

      // Register again with same ID
      messageHandler({
        data: {
          type: 'register',
          data: { tabId: 'tab1', config: {} }
        }
      });

      // Should have cleared the old timer and created a new one
      expect(global.clearInterval).toHaveBeenCalled();
      expect(global.setInterval.mock.calls.length).toBeGreaterThan(firstTimerCount);
    });
  });
});