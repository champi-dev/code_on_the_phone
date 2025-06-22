// Terminal Persistence Manager
// Handles WebSocket reconnection, keepalive, and background persistence

class TerminalPersistenceManager {
  constructor() {
    this.tabs = new Map();
    this.reconnectTimers = new Map();
    this.keepAliveTimers = new Map();
    this.visibilityState = 'visible';
    this.reconnectAttempts = new Map();
    this.maxReconnectAttempts = 10;
    this.baseReconnectDelay = 1000;
    this.maxReconnectDelay = 30000;
    this.keepAliveInterval = 30000;
    this.lastActivityTime = Date.now();
    this.connectionWorker = null;
    
    // Initialize event listeners
    this.initializeEventListeners();
    
    // Initialize service worker communication
    this.initializeServiceWorker();
    
    // Initialize connection worker
    this.initializeConnectionWorker();
  }

  initializeEventListeners() {
    // Page Visibility API
    document.addEventListener('visibilitychange', () => {
      this.handleVisibilityChange();
    });

    // Page lifecycle events
    window.addEventListener('pagehide', () => {
      this.handlePageHide();
    });

    window.addEventListener('pageshow', (event) => {
      if (event.persisted) {
        this.handlePageShow();
      }
    });

    // Network status
    window.addEventListener('online', () => {
      console.log('Network online - attempting reconnections');
      this.handleNetworkOnline();
    });

    window.addEventListener('offline', () => {
      console.log('Network offline');
      this.handleNetworkOffline();
    });

    // Mobile-specific events
    window.addEventListener('freeze', () => {
      this.handleFreeze();
    }, { capture: true });

    window.addEventListener('resume', () => {
      this.handleResume();
    }, { capture: true });

    // Unload event for cleanup
    window.addEventListener('beforeunload', () => {
      this.saveState();
    });

    // Focus/blur events
    window.addEventListener('focus', () => {
      this.handleWindowFocus();
    });

    window.addEventListener('blur', () => {
      this.handleWindowBlur();
    });
  }

  initializeServiceWorker() {
    if ('serviceWorker' in navigator && navigator.serviceWorker.controller) {
      // Listen for messages from service worker
      navigator.serviceWorker.addEventListener('message', (event) => {
        if (event.data.type === 'keepalive') {
          this.sendKeepAliveToAllTabs();
        } else if (event.data.type === 'reconnect') {
          this.reconnectAllTabs();
        }
      });

      // Send initialization message to service worker
      this.sendToServiceWorker({
        type: 'init',
        keepAliveInterval: this.keepAliveInterval
      });
    }
  }

  initializeConnectionWorker() {
    try {
      // Create dedicated worker for connection management
      this.connectionWorker = new Worker('/js/connection-worker.js');
      
      // Listen for messages from worker
      this.connectionWorker.addEventListener('message', (event) => {
        const { type, tabId } = event.data;
        
        switch (type) {
          case 'sendKeepAlive':
            this.sendKeepAlive(tabId);
            break;
          case 'needsReconnect':
            console.log(`Worker detected tab ${tabId} needs reconnection`);
            this.attemptReconnect(tabId);
            break;
          case 'status':
            console.log('Worker status:', event.data.data);
            break;
        }
      });
      
      console.log('Connection worker initialized');
    } catch (error) {
      console.error('Failed to initialize connection worker:', error);
    }
  }

  sendToServiceWorker(message) {
    if (navigator.serviceWorker.controller) {
      navigator.serviceWorker.controller.postMessage(message);
    }
  }

  registerTab(tabId, frameElement) {
    const tabData = {
      id: tabId,
      frame: frameElement,
      websocket: null,
      connected: false,
      lastPing: Date.now(),
      sessionData: null
    };

    this.tabs.set(tabId, tabData);
    this.setupWebSocketMonitoring(tabId);
    this.startKeepAlive(tabId);

    // Register with connection worker
    if (this.connectionWorker) {
      this.connectionWorker.postMessage({
        type: 'register',
        data: {
          tabId,
          config: {
            url: frameElement.src,
            keepAliveInterval: this.keepAliveInterval
          }
        }
      });
    }

    // Store initial session data
    this.saveTabSession(tabId);
  }

  unregisterTab(tabId) {
    this.stopKeepAlive(tabId);
    this.stopReconnectTimer(tabId);
    this.tabs.delete(tabId);
    this.reconnectAttempts.delete(tabId);

    // Unregister from connection worker
    if (this.connectionWorker) {
      this.connectionWorker.postMessage({
        type: 'unregister',
        data: { tabId }
      });
    }
  }

  setupWebSocketMonitoring(tabId) {
    const tab = this.tabs.get(tabId);
    if (!tab || !tab.frame || !tab.frame.contentWindow) return;

    try {
      // Monitor WebSocket connections in the iframe
      const frameWindow = tab.frame.contentWindow;
      
      // Override WebSocket constructor to intercept connections
      const OriginalWebSocket = frameWindow.WebSocket;
      frameWindow.WebSocket = function(...args) {
        const ws = new OriginalWebSocket(...args);
        
        // Store WebSocket reference
        tab.websocket = ws;
        
        // Monitor connection state
        ws.addEventListener('open', () => {
          console.log(`WebSocket opened for tab ${tabId}`);
          tab.connected = true;
          tab.lastPing = Date.now();
          this.stopReconnectTimer(tabId);
          this.reconnectAttempts.set(tabId, 0);
        });

        ws.addEventListener('close', (event) => {
          console.log(`WebSocket closed for tab ${tabId}`, event.code, event.reason);
          tab.connected = false;
          
          // Don't attempt reconnect if it was a clean close
          if (event.code !== 1000 && event.code !== 1001) {
            this.scheduleReconnect(tabId);
          }
        });

        ws.addEventListener('error', (error) => {
          console.error(`WebSocket error for tab ${tabId}`, error);
          tab.connected = false;
        });

        ws.addEventListener('message', () => {
          tab.lastPing = Date.now();
          this.lastActivityTime = Date.now();
        });

        return ws;
      };
    } catch (error) {
      console.error('Failed to setup WebSocket monitoring:', error);
    }
  }

  startKeepAlive(tabId) {
    // Clear existing timer
    this.stopKeepAlive(tabId);

    // Set up keepalive timer
    const timer = setInterval(() => {
      this.sendKeepAlive(tabId);
    }, this.keepAliveInterval);

    this.keepAliveTimers.set(tabId, timer);
  }

  stopKeepAlive(tabId) {
    const timer = this.keepAliveTimers.get(tabId);
    if (timer) {
      clearInterval(timer);
      this.keepAliveTimers.delete(tabId);
    }
  }

  sendKeepAlive(tabId) {
    const tab = this.tabs.get(tabId);
    if (!tab || !tab.websocket || tab.websocket.readyState !== WebSocket.OPEN) {
      return;
    }

    try {
      // Send ping frame (ttyd should respond with pong)
      tab.websocket.send('');
      tab.lastPing = Date.now();
    } catch (error) {
      console.error(`Failed to send keepalive for tab ${tabId}:`, error);
    }
  }

  sendKeepAliveToAllTabs() {
    for (const [tabId] of this.tabs) {
      this.sendKeepAlive(tabId);
    }
  }

  scheduleReconnect(tabId) {
    // Clear existing timer
    this.stopReconnectTimer(tabId);

    const attempts = this.reconnectAttempts.get(tabId) || 0;
    if (attempts >= this.maxReconnectAttempts) {
      console.error(`Max reconnection attempts reached for tab ${tabId}`);
      return;
    }

    // Calculate delay with exponential backoff
    const delay = Math.min(
      this.baseReconnectDelay * Math.pow(2, attempts),
      this.maxReconnectDelay
    );

    console.log(`Scheduling reconnect for tab ${tabId} in ${delay}ms (attempt ${attempts + 1})`);

    const timer = setTimeout(() => {
      this.attemptReconnect(tabId);
    }, delay);

    this.reconnectTimers.set(tabId, timer);
  }

  stopReconnectTimer(tabId) {
    const timer = this.reconnectTimers.get(tabId);
    if (timer) {
      clearTimeout(timer);
      this.reconnectTimers.delete(tabId);
    }
  }

  async attemptReconnect(tabId) {
    const tab = this.tabs.get(tabId);
    if (!tab || !tab.frame) return;

    const attempts = this.reconnectAttempts.get(tabId) || 0;
    this.reconnectAttempts.set(tabId, attempts + 1);

    console.log(`Attempting reconnect for tab ${tabId} (attempt ${attempts + 1})`);

    try {
      // Reload the iframe to establish new connection
      const currentSrc = tab.frame.src;
      
      // Add timestamp to force reload
      const url = new URL(currentSrc);
      url.searchParams.set('reconnect', Date.now());
      
      tab.frame.src = url.toString();

      // Wait for frame to load
      await new Promise((resolve) => {
        tab.frame.onload = resolve;
        setTimeout(resolve, 5000); // Timeout after 5s
      });

      // Re-setup WebSocket monitoring
      this.setupWebSocketMonitoring(tabId);

      // Restore session if available
      await this.restoreTabSession(tabId);

    } catch (error) {
      console.error(`Reconnection failed for tab ${tabId}:`, error);
      this.scheduleReconnect(tabId);
    }
  }

  reconnectAllTabs() {
    for (const [tabId] of this.tabs) {
      if (!this.isTabConnected(tabId)) {
        this.attemptReconnect(tabId);
      }
    }
  }

  isTabConnected(tabId) {
    const tab = this.tabs.get(tabId);
    return tab && tab.connected && tab.websocket && 
           tab.websocket.readyState === WebSocket.OPEN;
  }

  saveTabSession(tabId) {
    const tab = this.tabs.get(tabId);
    if (!tab || !tab.frame) return;

    try {
      // Save terminal state
      const sessionData = {
        tabId,
        timestamp: Date.now(),
        url: tab.frame.src,
        // Add more session data as needed
      };

      tab.sessionData = sessionData;

      // Save to localStorage
      const sessions = this.getSavedSessions();
      sessions[tabId] = sessionData;
      localStorage.setItem('terminal-sessions', JSON.stringify(sessions));
    } catch (error) {
      console.error('Failed to save tab session:', error);
    }
  }

  async restoreTabSession(tabId) {
    const tab = this.tabs.get(tabId);
    if (!tab) return;

    const sessions = this.getSavedSessions();
    const sessionData = sessions[tabId] || tab.sessionData;

    if (sessionData) {
      console.log(`Restoring session for tab ${tabId}`);
      // Implement session restoration logic here
      // This might include sending commands to restore terminal state
    }
  }

  getSavedSessions() {
    try {
      return JSON.parse(localStorage.getItem('terminal-sessions') || '{}');
    } catch {
      return {};
    }
  }

  saveState() {
    const state = {
      tabs: Array.from(this.tabs.keys()),
      lastActivity: this.lastActivityTime,
      timestamp: Date.now()
    };

    try {
      sessionStorage.setItem('terminal-state', JSON.stringify(state));
    } catch (error) {
      console.error('Failed to save state:', error);
    }
  }

  restoreState() {
    try {
      const state = JSON.parse(sessionStorage.getItem('terminal-state') || '{}');
      
      if (state.timestamp && Date.now() - state.timestamp < 300000) { // 5 minutes
        return state;
      }
    } catch (error) {
      console.error('Failed to restore state:', error);
    }
    return null;
  }

  // Visibility and lifecycle handlers
  handleVisibilityChange() {
    const wasHidden = this.visibilityState === 'hidden';
    this.visibilityState = document.visibilityState;

    if (document.visibilityState === 'visible' && wasHidden) {
      console.log('Page became visible - checking connections');
      this.handlePageVisible();
    } else if (document.visibilityState === 'hidden') {
      console.log('Page became hidden - maintaining connections');
      this.handlePageHidden();
    }
  }

  handlePageVisible() {
    // Check all connections and reconnect if needed
    for (const [tabId] of this.tabs) {
      if (!this.isTabConnected(tabId)) {
        this.attemptReconnect(tabId);
      } else {
        // Send keepalive to ensure connection is still valid
        this.sendKeepAlive(tabId);
      }
    }

    // Increase keepalive frequency when visible
    this.updateKeepAliveFrequency(30000);
  }

  handlePageHidden() {
    // Save current state
    this.saveState();

    // Reduce keepalive frequency to conserve battery
    this.updateKeepAliveFrequency(60000);

    // Send immediate keepalive to all tabs
    this.sendKeepAliveToAllTabs();

    // Notify service worker
    this.sendToServiceWorker({
      type: 'background',
      tabs: Array.from(this.tabs.keys())
    });
  }

  handlePageHide() {
    // Page is being unloaded or put in bfcache
    this.saveState();
    this.sendKeepAliveToAllTabs();
  }

  handlePageShow() {
    // Page restored from bfcache
    console.log('Page restored from cache - checking connections');
    this.handlePageVisible();
  }

  handleFreeze() {
    // Page is being frozen (mobile background)
    console.log('Page freezing - saving state');
    this.saveState();
    this.sendKeepAliveToAllTabs();
  }

  handleResume() {
    // Page resumed from frozen state
    console.log('Page resumed - restoring connections');
    this.handlePageVisible();
  }

  handleNetworkOnline() {
    // Network connection restored
    setTimeout(() => {
      this.reconnectAllTabs();
    }, 1000);
  }

  handleNetworkOffline() {
    // Network connection lost
    for (const [tabId] of this.tabs) {
      this.stopReconnectTimer(tabId);
    }
  }

  handleWindowFocus() {
    // Window gained focus
    if (Date.now() - this.lastActivityTime > 60000) {
      // Check connections if inactive for more than 1 minute
      this.handlePageVisible();
    }
  }

  handleWindowBlur() {
    // Window lost focus
    this.lastActivityTime = Date.now();
  }

  updateKeepAliveFrequency(interval) {
    this.keepAliveInterval = interval;
    
    // Restart all keepalive timers with new interval
    for (const [tabId] of this.tabs) {
      this.startKeepAlive(tabId);
    }

    // Update service worker
    this.sendToServiceWorker({
      type: 'updateInterval',
      keepAliveInterval: interval
    });
  }
}

// Export for use in main application
window.TerminalPersistenceManager = TerminalPersistenceManager;