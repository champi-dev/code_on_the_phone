// Service Worker for PWA functionality
const CACHE_NAME = 'cloud-terminal-v12';
const urlsToCache = [
  '/manifest.json',
  '/icon.svg',
  '/js/three.min.js',
  '/js/terminal-persistence.js'
];

// Background sync and keepalive state
let keepAliveInterval = 30000;
let keepAliveTimer = null;
let registeredTabs = new Set();

// Install event - cache resources
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then(cache => {
        console.log('Opened cache');
        // Cache resources one by one to handle failures gracefully
        return Promise.all(
          urlsToCache.map(url => {
            return cache.add(url).catch(err => {
              console.warn(`Failed to cache ${url}:`, err);
              // Continue caching other resources even if one fails
            });
          })
        );
      })
  );
  self.skipWaiting();
});

// Activate event - cleanup old caches
self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.map(cacheName => {
          if (cacheName !== CACHE_NAME) {
            console.log('Deleting old cache:', cacheName);
            return caches.delete(cacheName);
          }
        })
      );
    })
  );
  self.clients.claim();
});

// Fetch event - serve from cache when offline
self.addEventListener('fetch', event => {
  // Skip non-GET requests
  if (event.request.method !== 'GET') return;

  // Skip API requests and terminal iframe
  if (event.request.url.includes('/api/') || 
      event.request.url.includes(':7681')) {
    return;
  }

  event.respondWith(
    caches.match(event.request)
      .then(response => {
        // Cache hit - return response
        if (response) {
          return response;
        }

        // Clone the request
        const fetchRequest = event.request.clone();

        return fetch(fetchRequest).then(response => {
          // Check if valid response
          if (!response || response.status !== 200 || response.type !== 'basic') {
            return response;
          }

          // Clone the response
          const responseToCache = response.clone();

          caches.open(CACHE_NAME)
            .then(cache => {
              cache.put(event.request, responseToCache);
            });

          return response;
        });
      })
      .catch(() => {
        // Offline fallback
        if (event.request.destination === 'document') {
          return caches.match('/');
        }
      })
  );
});

// Background sync for offline commands
self.addEventListener('sync', event => {
  if (event.tag === 'sync-commands') {
    event.waitUntil(syncCommands());
  } else if (event.tag === 'keepalive') {
    event.waitUntil(sendKeepAliveToClients());
  }
});

async function syncCommands() {
  // Sync any queued terminal commands when back online
  console.log('Syncing offline commands...');
}

// Message handling from clients
self.addEventListener('message', event => {
  const { type, data } = event.data || {};
  
  switch (type) {
    case 'init':
      handleInit(event.data);
      break;
    case 'updateInterval':
      keepAliveInterval = event.data.keepAliveInterval || 30000;
      restartKeepAliveTimer();
      break;
    case 'background':
      handleBackground(event.data);
      break;
    case 'registerTab':
      registeredTabs.add(event.data.tabId);
      break;
    case 'unregisterTab':
      registeredTabs.delete(event.data.tabId);
      break;
  }
});

function handleInit(data) {
  keepAliveInterval = data.keepAliveInterval || 30000;
  startKeepAliveTimer();
}

function handleBackground(data) {
  if (data.tabs) {
    data.tabs.forEach(tabId => registeredTabs.add(tabId));
  }
  // Continue keepalive in background
  restartKeepAliveTimer();
}

function startKeepAliveTimer() {
  if (keepAliveTimer) return;
  
  keepAliveTimer = setInterval(() => {
    sendKeepAliveToClients();
  }, keepAliveInterval);
}

function stopKeepAliveTimer() {
  if (keepAliveTimer) {
    clearInterval(keepAliveTimer);
    keepAliveTimer = null;
  }
}

function restartKeepAliveTimer() {
  stopKeepAliveTimer();
  startKeepAliveTimer();
}

async function sendKeepAliveToClients() {
  const clients = await self.clients.matchAll({
    includeUncontrolled: true,
    type: 'window'
  });
  
  clients.forEach(client => {
    client.postMessage({
      type: 'keepalive',
      timestamp: Date.now()
    });
  });
}

// Periodic sync for background updates
self.addEventListener('periodicsync', event => {
  if (event.tag === 'terminal-keepalive') {
    event.waitUntil(handlePeriodicSync());
  }
});

async function handlePeriodicSync() {
  console.log('Periodic sync: maintaining terminal connections');
  await sendKeepAliveToClients();
  
  // Check if we should trigger reconnections
  const clients = await self.clients.matchAll({
    includeUncontrolled: true,
    type: 'window'
  });
  
  if (clients.length > 0) {
    clients.forEach(client => {
      client.postMessage({
        type: 'reconnect',
        timestamp: Date.now()
      });
    });
  }
}