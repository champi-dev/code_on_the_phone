const http = require('http');
const WebSocket = require('ws');

const TERMINAL_HOST = process.env.TERMINAL_HOST || '142.93.249.123';
const TERMINAL_PORT = process.env.TERMINAL_PORT || '7681';

console.log(`Testing connection to ${TERMINAL_HOST}:${TERMINAL_PORT}`);

// Test HTTP connection
const testHttp = () => {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: TERMINAL_HOST,
      port: TERMINAL_PORT,
      path: '/',
      method: 'GET',
      timeout: 5000
    };

    const req = http.request(options, (res) => {
      console.log(`HTTP Status: ${res.statusCode}`);
      console.log(`Headers:`, res.headers);
      resolve(res.statusCode === 200);
    });

    req.on('error', (err) => {
      console.error('HTTP Error:', err.message);
      resolve(false);
    });

    req.on('timeout', () => {
      console.error('HTTP Timeout');
      req.destroy();
      resolve(false);
    });

    req.end();
  });
};

// Test WebSocket connection
const testWebSocket = () => {
  return new Promise((resolve) => {
    const wsUrl = `ws://${TERMINAL_HOST}:${TERMINAL_PORT}/ws`;
    console.log(`Testing WebSocket: ${wsUrl}`);
    
    const ws = new WebSocket(wsUrl);
    let connected = false;

    ws.on('open', () => {
      console.log('WebSocket connected!');
      connected = true;
      ws.close();
    });

    ws.on('message', (data) => {
      console.log('WebSocket message:', data.toString().substring(0, 100));
    });

    ws.on('error', (err) => {
      console.error('WebSocket error:', err.message);
    });

    ws.on('close', () => {
      console.log('WebSocket closed');
      resolve(connected);
    });

    // Timeout after 5 seconds
    setTimeout(() => {
      if (!connected) {
        console.log('WebSocket timeout');
        ws.close();
        resolve(false);
      }
    }, 5000);
  });
};

// Test production site
const testProduction = () => {
  return new Promise((resolve) => {
    const https = require('https');
    
    https.get('https://code-on-the-phone.onrender.com/api/terminal-health', (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log('Production health check:', res.statusCode, data);
        resolve(res.statusCode === 200 || res.statusCode === 401);
      });
    }).on('error', (err) => {
      console.error('Production error:', err.message);
      resolve(false);
    });
  });
};

// Run tests
(async () => {
  console.log('\n=== Testing Terminal Connection ===\n');
  
  const httpOk = await testHttp();
  console.log(`\nHTTP Test: ${httpOk ? 'PASSED' : 'FAILED'}`);
  
  const wsOk = await testWebSocket();
  console.log(`WebSocket Test: ${wsOk ? 'PASSED' : 'FAILED'}`);
  
  console.log('\n=== Testing Production Site ===\n');
  
  const prodOk = await testProduction();
  console.log(`Production Test: ${prodOk ? 'PASSED' : 'FAILED'}`);
  
  console.log('\n=== Summary ===');
  console.log(`Terminal accessible: ${httpOk ? 'YES' : 'NO'}`);
  console.log(`WebSocket works: ${wsOk ? 'YES' : 'NO'}`);
  console.log(`Production site up: ${prodOk ? 'YES' : 'NO'}`);
  
  process.exit(0);
})();