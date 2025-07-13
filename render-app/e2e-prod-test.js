const https = require('https');
const WebSocket = require('ws');

async function fullE2ETest() {
  console.log('=== FULL E2E PRODUCTION TEST ===\n');
  
  const password = 'cloudterm123';
  let sessionCookie = null;
  
  // Step 1: Test Homepage
  console.log('1. Testing homepage...');
  await new Promise((resolve) => {
    https.get('https://code-on-the-phone.onrender.com/', (res) => {
      console.log(`   Status: ${res.statusCode}`);
      console.log(`   Headers:`, res.headers['content-type']);
      resolve();
    });
  });
  
  // Step 2: Login
  console.log('\n2. Testing login...');
  const loginData = JSON.stringify({ password });
  
  sessionCookie = await new Promise((resolve) => {
    const req = https.request({
      hostname: 'code-on-the-phone.onrender.com',
      path: '/api/login',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': loginData.length
      }
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        console.log(`   Status: ${res.statusCode}`);
        console.log(`   Response:`, data);
        
        if (res.headers['set-cookie']) {
          const cookie = res.headers['set-cookie'].find(c => c.startsWith('sessionId='));
          resolve(cookie ? cookie.split(';')[0] : null);
        } else {
          resolve(null);
        }
      });
    });
    
    req.write(loginData);
    req.end();
  });
  
  if (!sessionCookie) {
    console.error('   ❌ Login failed!');
    return;
  }
  
  console.log(`   ✓ Got session: ${sessionCookie.substring(0, 30)}...`);
  
  // Step 3: Check Terminal Config
  console.log('\n3. Checking terminal configuration...');
  const terminalConfig = await new Promise((resolve) => {
    https.get({
      hostname: 'code-on-the-phone.onrender.com',
      path: '/api/terminal-config',
      headers: { 'Cookie': sessionCookie }
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        console.log(`   Status: ${res.statusCode}`);
        const config = JSON.parse(data);
        console.log(`   Config:`, config);
        resolve(config);
      });
    });
  });
  
  // Step 4: Test Terminal Health
  console.log('\n4. Testing terminal health check...');
  await new Promise((resolve) => {
    https.get({
      hostname: 'code-on-the-phone.onrender.com',
      path: '/api/terminal-health',
      headers: { 'Cookie': sessionCookie }
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        console.log(`   Status: ${res.statusCode}`);
        console.log(`   Health:`, data);
        resolve();
      });
    });
  });
  
  // Step 5: Test Direct Droplet Connection
  console.log('\n5. Testing direct droplet connection...');
  const dropletTest = await new Promise((resolve) => {
    const http = require('http');
    http.get('http://142.93.249.123:7681/', (res) => {
      console.log(`   Droplet HTTP: ${res.statusCode}`);
      resolve(true);
    }).on('error', (err) => {
      console.log(`   Droplet Error: ${err.message}`);
      resolve(false);
    });
  });
  
  // Step 6: Test ttyd-terminal.html loading
  console.log('\n6. Testing ttyd-terminal.html...');
  await new Promise((resolve) => {
    https.get({
      hostname: 'code-on-the-phone.onrender.com',
      path: '/ttyd-terminal.html',
      headers: { 'Cookie': sessionCookie }
    }, (res) => {
      console.log(`   Status: ${res.statusCode}`);
      console.log(`   Content-Type:`, res.headers['content-type']);
      resolve();
    });
  });
  
  // Step 7: Test WebSocket through proxy
  console.log('\n7. Testing WebSocket connection through proxy...');
  
  const wsUrl = 'wss://code-on-the-phone.onrender.com/terminal-proxy/ws';
  console.log(`   Connecting to: ${wsUrl}`);
  
  const ws = new WebSocket(wsUrl, {
    headers: { 'Cookie': sessionCookie }
  });
  
  await new Promise((resolve) => {
    let connected = false;
    
    ws.on('open', () => {
      console.log('   ✓ WebSocket connected!');
      connected = true;
      
      // For ttyd, send a test input (binary frame)
      const testInput = Buffer.from([0, 'l'.charCodeAt(0), 's'.charCodeAt(0), '\n'.charCodeAt(0)]);
      ws.send(testInput);
      
      setTimeout(() => {
        ws.close();
        resolve();
      }, 2000);
    });
    
    ws.on('message', (data) => {
      console.log(`   Received: ${data.toString().substring(0, 100)}...`);
    });
    
    ws.on('error', (err) => {
      console.log(`   ❌ WebSocket error: ${err.message}`);
      resolve();
    });
    
    ws.on('close', (code, reason) => {
      console.log(`   WebSocket closed: ${code} ${reason}`);
      if (!connected) resolve();
    });
    
    // Timeout
    setTimeout(() => {
      if (!connected) {
        console.log('   ❌ WebSocket timeout');
        ws.close();
        resolve();
      }
    }, 10000);
  });
  
  // Step 8: Test proxy endpoint directly
  console.log('\n8. Testing proxy endpoint...');
  await new Promise((resolve) => {
    https.get({
      hostname: 'code-on-the-phone.onrender.com',
      path: '/terminal-proxy',
      headers: { 'Cookie': sessionCookie }
    }, (res) => {
      console.log(`   Status: ${res.statusCode}`);
      console.log(`   Headers:`, res.headers);
      
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        console.log(`   Body preview:`, data.substring(0, 200));
        resolve();
      });
    });
  });
  
  console.log('\n=== TEST COMPLETE ===');
}

fullE2ETest().catch(console.error);