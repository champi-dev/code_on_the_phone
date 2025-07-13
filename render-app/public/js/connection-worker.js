// Web Worker for maintaining terminal connections in background
// This worker runs independently of the main thread

let connections = new Map();
let keepAliveInterval = 30000;
let keepAliveTimers = new Map();

// Message handler from main thread
self.addEventListener('message', (event) => {
  const { type, data } = event.data;
  
  switch (type) {
    case 'register':
      if (data && data.tabId && data.config) {
        registerConnection(data.tabId, data.config);
      }
      break;
    case 'unregister':
      if (data && data.tabId) {
        unregisterConnection(data.tabId);
      }
      break;
    case 'keepalive':
      if (data && data.tabId) {
        sendKeepAlive(data.tabId);
      }
      break;
    case 'updateInterval':
      if (data && data.interval) {
        updateKeepAliveInterval(data.interval);
      }
      break;
    case 'status':
      reportStatus();
      break;
  }
});

function registerConnection(tabId, config) {
  console.log(`[Worker] Registering connection for tab ${tabId}`);
  
  const connection = {
    tabId,
    config,
    lastPing: Date.now(),
    active: true
  };
  
  connections.set(tabId, connection);
  startKeepAliveTimer(tabId);
  
  // Notify main thread
  self.postMessage({
    type: 'registered',
    tabId,
    timestamp: Date.now()
  });
}

function unregisterConnection(tabId) {
  console.log(`[Worker] Unregistering connection for tab ${tabId}`);
  
  stopKeepAliveTimer(tabId);
  connections.delete(tabId);
  
  // Notify main thread
  self.postMessage({
    type: 'unregistered',
    tabId,
    timestamp: Date.now()
  });
}

function startKeepAliveTimer(tabId) {
  // Clear existing timer
  stopKeepAliveTimer(tabId);
  
  // Create new timer
  const timer = setInterval(() => {
    const connection = connections.get(tabId);
    if (connection && connection.active) {
      sendKeepAlive(tabId);
    }
  }, keepAliveInterval);
  
  keepAliveTimers.set(tabId, timer);
}

function stopKeepAliveTimer(tabId) {
  const timer = keepAliveTimers.get(tabId);
  if (timer) {
    clearInterval(timer);
    keepAliveTimers.delete(tabId);
  }
}

function sendKeepAlive(tabId) {
  const connection = connections.get(tabId);
  if (!connection) return;
  
  connection.lastPing = Date.now();
  
  // Notify main thread to send keepalive
  self.postMessage({
    type: 'sendKeepAlive',
    tabId,
    timestamp: Date.now()
  });
}

function updateKeepAliveInterval(interval) {
  keepAliveInterval = interval;
  
  // Restart all timers with new interval
  for (const [tabId] of connections) {
    startKeepAliveTimer(tabId);
  }
}

function reportStatus() {
  const status = {
    connections: Array.from(connections.entries()).map(([tabId, conn]) => ({
      tabId,
      lastPing: conn.lastPing,
      active: conn.active,
      uptime: Date.now() - conn.lastPing
    })),
    keepAliveInterval
  };
  
  self.postMessage({
    type: 'status',
    data: status,
    timestamp: Date.now()
  });
}

// Periodic health check
setInterval(() => {
  const now = Date.now();
  
  for (const [tabId, connection] of connections) {
    const timeSinceLastPing = now - connection.lastPing;
    
    // If no activity for 2 minutes, trigger reconnection
    if (timeSinceLastPing > 120000) {
      self.postMessage({
        type: 'needsReconnect',
        tabId,
        lastPing: connection.lastPing,
        timestamp: now
      });
    }
  }
}, 60000); // Check every minute

console.log('[Worker] Connection worker initialized');