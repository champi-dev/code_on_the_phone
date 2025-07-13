const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');
const { JSDOM } = require('jsdom');

describe('TerminalPersistenceManager Tests', () => {
  let dom;
  let window;
  let document;
  let manager;
  let mockWorker;
  let mockServiceWorker;

  beforeEach(() => {
    // Setup JSDOM
    dom = new JSDOM('<!DOCTYPE html><html><body></body></html>', {
      url: 'http://localhost',
      pretendToBeVisual: true,
      resources: 'usable'
    });

    window = dom.window;
    document = window.document;
    global.window = window;
    global.document = document;
    global.WebSocket = jest.fn();
    global.Worker = jest.fn();
    global.navigator = window.navigator;
    global.localStorage = {
      getItem: jest.fn(),
      setItem: jest.fn(),
      removeItem: jest.fn()
    };
    global.sessionStorage = {
      getItem: jest.fn(),
      setItem: jest.fn(),
      removeItem: jest.fn()
    };

    // Mock Worker
    mockWorker = {
      postMessage: jest.fn(),
      addEventListener: jest.fn(),
      terminate: jest.fn()
    };
    global.Worker.mockReturnValue(mockWorker);

    // Mock Service Worker
    mockServiceWorker = {
      controller: {
        postMessage: jest.fn()
      },
      addEventListener: jest.fn()
    };
    Object.defineProperty(window.navigator, 'serviceWorker', {
      value: mockServiceWorker,
      writable: true
    });

    // Load the script
    const script = require('fs').readFileSync(
      require('path').join(__dirname, '../public/js/terminal-persistence.js'),
      'utf8'
    );
    const scriptEl = document.createElement('script');
    scriptEl.textContent = script;
    document.head.appendChild(scriptEl);

    // Create manager instance
    manager = new window.TerminalPersistenceManager();
  });

  afterEach(() => {
    jest.clearAllMocks();
    delete global.window;
    delete global.document;
    delete global.WebSocket;
    delete global.Worker;
    delete global.navigator;
    delete global.localStorage;
    delete global.sessionStorage;
  });

  describe('Initialization', () => {
    it('should initialize with default values', () => {
      expect(manager.tabs).toBeDefined();
      expect(manager.tabs.size).toBe(0);
      expect(manager.maxReconnectAttempts).toBe(10);
      expect(manager.baseReconnectDelay).toBe(1000);
      expect(manager.keepAliveInterval).toBe(30000);
    });

    it('should initialize event listeners', () => {
      const addEventListenerSpy = jest.spyOn(document, 'addEventListener');
      const windowAddEventListenerSpy = jest.spyOn(window, 'addEventListener');
      
      new window.TerminalPersistenceManager();
      
      expect(addEventListenerSpy).toHaveBeenCalledWith('visibilitychange', expect.any(Function));
      expect(windowAddEventListenerSpy).toHaveBeenCalledWith('pagehide', expect.any(Function));
      expect(windowAddEventListenerSpy).toHaveBeenCalledWith('pageshow', expect.any(Function));
      expect(windowAddEventListenerSpy).toHaveBeenCalledWith('online', expect.any(Function));
      expect(windowAddEventListenerSpy).toHaveBeenCalledWith('offline', expect.any(Function));
    });

    it('should initialize service worker communication', () => {
      expect(mockServiceWorker.addEventListener).toHaveBeenCalledWith('message', expect.any(Function));
      expect(mockServiceWorker.controller.postMessage).toHaveBeenCalledWith({
        type: 'init',
        keepAliveInterval: 30000
      });
    });

    it('should initialize connection worker', () => {
      expect(global.Worker).toHaveBeenCalledWith('/js/connection-worker.js');
      expect(mockWorker.addEventListener).toHaveBeenCalledWith('message', expect.any(Function));
    });

    it('should handle Worker initialization failure', () => {
      global.Worker.mockImplementation(() => {
        throw new Error('Worker not supported');
      });
      
      const consoleErrorSpy = jest.spyOn(console, 'error').mockImplementation();
      new window.TerminalPersistenceManager();
      
      expect(consoleErrorSpy).toHaveBeenCalledWith('Failed to initialize connection worker:', expect.any(Error));
    });
  });

  describe('Tab Management', () => {
    let mockFrame;

    beforeEach(() => {
      mockFrame = document.createElement('iframe');
      mockFrame.src = 'http://localhost/terminal';
      document.body.appendChild(mockFrame);
    });

    it('should register a tab', () => {
      manager.registerTab('tab1', mockFrame);
      
      expect(manager.tabs.has('tab1')).toBe(true);
      const tab = manager.tabs.get('tab1');
      expect(tab.id).toBe('tab1');
      expect(tab.frame).toBe(mockFrame);
      expect(tab.connected).toBe(false);
    });

    it('should unregister a tab', () => {
      manager.registerTab('tab1', mockFrame);
      manager.unregisterTab('tab1');
      
      expect(manager.tabs.has('tab1')).toBe(false);
      expect(mockWorker.postMessage).toHaveBeenCalledWith({
        type: 'unregister',
        data: { tabId: 'tab1' }
      });
    });

    it('should save tab session on registration', () => {
      localStorage.getItem.mockReturnValue('{}');
      
      manager.registerTab('tab1', mockFrame);
      
      expect(localStorage.setItem).toHaveBeenCalledWith(
        'terminal-sessions',
        expect.stringContaining('"tab1"')
      );
    });
  });

  describe('WebSocket Monitoring', () => {
    it('should setup WebSocket monitoring for a tab', () => {
      const mockFrame = {
        src: 'http://localhost/terminal',
        contentWindow: {
          WebSocket: WebSocket
        }
      };
      
      manager.registerTab('tab1', mockFrame);
      manager.setupWebSocketMonitoring('tab1');
      
      // Create a WebSocket through the monitored constructor
      const ws = new mockFrame.contentWindow.WebSocket('ws://localhost');
      
      expect(manager.tabs.get('tab1').websocket).toBe(ws);
    });

    it('should handle WebSocket connection events', () => {
      const mockWs = {
        addEventListener: jest.fn(),
        readyState: WebSocket.OPEN
      };
      global.WebSocket.mockImplementation(() => mockWs);
      
      const mockFrame = {
        src: 'http://localhost/terminal',
        contentWindow: { WebSocket: global.WebSocket }
      };
      
      manager.registerTab('tab1', mockFrame);
      manager.setupWebSocketMonitoring('tab1');
      
      // Verify event listeners were added
      expect(mockWs.addEventListener).toHaveBeenCalledWith('open', expect.any(Function));
      expect(mockWs.addEventListener).toHaveBeenCalledWith('close', expect.any(Function));
      expect(mockWs.addEventListener).toHaveBeenCalledWith('error', expect.any(Function));
      expect(mockWs.addEventListener).toHaveBeenCalledWith('message', expect.any(Function));
    });
  });

  describe('Keep Alive Functionality', () => {
    beforeEach(() => {
      jest.useFakeTimers();
    });

    afterEach(() => {
      jest.useRealTimers();
    });

    it('should start keep alive timer for a tab', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      
      expect(manager.keepAliveTimers.has('tab1')).toBe(true);
    });

    it('should send keep alive messages at intervals', () => {
      const mockWs = {
        readyState: WebSocket.OPEN,
        send: jest.fn()
      };
      
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      manager.tabs.get('tab1').websocket = mockWs;
      
      jest.advanceTimersByTime(30000);
      
      expect(mockWs.send).toHaveBeenCalledWith('');
    });

    it('should stop keep alive timer', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      
      const clearIntervalSpy = jest.spyOn(global, 'clearInterval');
      manager.stopKeepAlive('tab1');
      
      expect(clearIntervalSpy).toHaveBeenCalled();
      expect(manager.keepAliveTimers.has('tab1')).toBe(false);
    });
  });

  describe('Reconnection Logic', () => {
    beforeEach(() => {
      jest.useFakeTimers();
    });

    afterEach(() => {
      jest.useRealTimers();
    });

    it('should schedule reconnect with exponential backoff', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      
      manager.scheduleReconnect('tab1');
      expect(manager.reconnectTimers.has('tab1')).toBe(true);
      
      // First attempt - 1000ms delay
      jest.advanceTimersByTime(1000);
      
      // Set attempt count
      manager.reconnectAttempts.set('tab1', 1);
      manager.scheduleReconnect('tab1');
      
      // Second attempt - 2000ms delay (exponential backoff)
      const timer = manager.reconnectTimers.get('tab1');
      expect(timer).toBeDefined();
    });

    it('should not exceed max reconnect attempts', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      manager.reconnectAttempts.set('tab1', 10);
      
      const consoleErrorSpy = jest.spyOn(console, 'error').mockImplementation();
      manager.scheduleReconnect('tab1');
      
      expect(consoleErrorSpy).toHaveBeenCalledWith('Max reconnection attempts reached for tab tab1');
      expect(manager.reconnectTimers.has('tab1')).toBe(false);
    });

    it('should attempt reconnect by reloading iframe', async () => {
      const mockFrame = document.createElement('iframe');
      mockFrame.src = 'http://localhost/terminal';
      manager.registerTab('tab1', mockFrame);
      
      await manager.attemptReconnect('tab1');
      
      expect(mockFrame.src).toContain('reconnect=');
    });
  });

  describe('Visibility and Lifecycle Handlers', () => {
    it('should handle page becoming visible', () => {
      Object.defineProperty(document, 'visibilityState', {
        value: 'hidden',
        writable: true
      });
      
      manager.visibilityState = 'hidden';
      manager.handleVisibilityChange();
      
      Object.defineProperty(document, 'visibilityState', {
        value: 'visible',
        writable: true
      });
      
      const attemptReconnectSpy = jest.spyOn(manager, 'attemptReconnect').mockImplementation();
      manager.handleVisibilityChange();
      
      expect(manager.visibilityState).toBe('visible');
    });

    it('should handle page becoming hidden', () => {
      Object.defineProperty(document, 'visibilityState', {
        value: 'visible',
        writable: true
      });
      
      manager.handleVisibilityChange();
      
      Object.defineProperty(document, 'visibilityState', {
        value: 'hidden',
        writable: true
      });
      
      const saveStateSpy = jest.spyOn(manager, 'saveState').mockImplementation();
      manager.handleVisibilityChange();
      
      expect(saveStateSpy).toHaveBeenCalled();
      expect(manager.visibilityState).toBe('hidden');
    });

    it('should handle network coming online', () => {
      jest.useFakeTimers();
      const reconnectAllTabsSpy = jest.spyOn(manager, 'reconnectAllTabs').mockImplementation();
      
      manager.handleNetworkOnline();
      jest.advanceTimersByTime(1000);
      
      expect(reconnectAllTabsSpy).toHaveBeenCalled();
      jest.useRealTimers();
    });

    it('should handle network going offline', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      manager.reconnectTimers.set('tab1', setTimeout(() => {}, 1000));
      
      manager.handleNetworkOffline();
      
      expect(manager.reconnectTimers.has('tab1')).toBe(false);
    });
  });

  describe('State Persistence', () => {
    it('should save state to sessionStorage', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      manager.registerTab('tab2', mockFrame);
      
      manager.saveState();
      
      expect(sessionStorage.setItem).toHaveBeenCalledWith(
        'terminal-state',
        expect.stringContaining('"tabs":["tab1","tab2"]')
      );
    });

    it('should restore state from sessionStorage', () => {
      const state = {
        tabs: ['tab1', 'tab2'],
        lastActivity: Date.now() - 1000,
        timestamp: Date.now() - 1000
      };
      
      sessionStorage.getItem.mockReturnValue(JSON.stringify(state));
      
      const restoredState = manager.restoreState();
      expect(restoredState).toEqual(state);
    });

    it('should not restore stale state', () => {
      const staleState = {
        timestamp: Date.now() - 600000 // 10 minutes old
      };
      
      sessionStorage.getItem.mockReturnValue(JSON.stringify(staleState));
      
      const restoredState = manager.restoreState();
      expect(restoredState).toBeNull();
    });

    it('should handle localStorage errors gracefully', () => {
      localStorage.getItem.mockImplementation(() => {
        throw new Error('Storage error');
      });
      
      const sessions = manager.getSavedSessions();
      expect(sessions).toEqual({});
    });
  });

  describe('Service Worker Communication', () => {
    it('should send messages to service worker', () => {
      manager.sendToServiceWorker({ type: 'test' });
      
      expect(mockServiceWorker.controller.postMessage).toHaveBeenCalledWith({ type: 'test' });
    });

    it('should handle service worker messages', () => {
      const sendKeepAliveToAllTabsSpy = jest.spyOn(manager, 'sendKeepAliveToAllTabs').mockImplementation();
      
      // Simulate service worker message
      const messageHandler = mockServiceWorker.addEventListener.mock.calls[0][1];
      messageHandler({ data: { type: 'keepalive' } });
      
      expect(sendKeepAliveToAllTabsSpy).toHaveBeenCalled();
    });

    it('should handle no service worker controller', () => {
      delete navigator.serviceWorker.controller;
      
      // Should not throw
      expect(() => manager.sendToServiceWorker({ type: 'test' })).not.toThrow();
    });
  });

  describe('Connection Worker Communication', () => {
    it('should handle worker messages', () => {
      const sendKeepAliveSpy = jest.spyOn(manager, 'sendKeepAlive').mockImplementation();
      
      // Get message handler
      const messageHandler = mockWorker.addEventListener.mock.calls[0][1];
      
      // Test sendKeepAlive message
      messageHandler({ data: { type: 'sendKeepAlive', tabId: 'tab1' } });
      expect(sendKeepAliveSpy).toHaveBeenCalledWith('tab1');
      
      // Test needsReconnect message
      const attemptReconnectSpy = jest.spyOn(manager, 'attemptReconnect').mockImplementation();
      messageHandler({ data: { type: 'needsReconnect', tabId: 'tab1' } });
      expect(attemptReconnectSpy).toHaveBeenCalledWith('tab1');
    });
  });

  describe('Keep Alive Frequency Update', () => {
    it('should update keep alive frequency for all tabs', () => {
      const mockFrame = document.createElement('iframe');
      manager.registerTab('tab1', mockFrame);
      manager.registerTab('tab2', mockFrame);
      
      const startKeepAliveSpy = jest.spyOn(manager, 'startKeepAlive');
      manager.updateKeepAliveFrequency(60000);
      
      expect(manager.keepAliveInterval).toBe(60000);
      expect(startKeepAliveSpy).toHaveBeenCalledWith('tab1');
      expect(startKeepAliveSpy).toHaveBeenCalledWith('tab2');
    });
  });
});