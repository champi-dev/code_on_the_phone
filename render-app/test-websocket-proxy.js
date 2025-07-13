const WebSocket = require('ws');
const https = require('https');

async function getSessionCookie() {
  const password = 'cloudterm123';
  const loginData = JSON.stringify({ password });
  
  const loginOptions = {
    hostname: 'code-on-the-phone.onrender.com',
    path: '/api/login',
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': loginData.length
    }
  };
  
  return new Promise((resolve, reject) => {
    const req = https.request(loginOptions, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        if (res.statusCode === 200) {
          const cookies = res.headers['set-cookie'];
          if (cookies) {
            const sessionId = cookies.find(c => c.startsWith('sessionId='));
            if (sessionId) {
              resolve(sessionId.split(';')[0]);
            }
          }
        }
        resolve(null);
      });
    });
    
    req.on('error', reject);
    req.write(loginData);
    req.end();
  });
}

async function testWebSocketProxy() {
  console.log('Getting session cookie...');
  const sessionCookie = await getSessionCookie();
  
  if (!sessionCookie) {
    console.error('Failed to get session');
    return;
  }
  
  console.log('Got session:', sessionCookie);
  
  // Test WebSocket connection through proxy
  const wsUrl = 'wss://code-on-the-phone.onrender.com/terminal-proxy';
  console.log('\nTesting WebSocket connection to:', wsUrl);
  
  const ws = new WebSocket(wsUrl, {
    headers: {
      'Cookie': sessionCookie
    }
  });
  
  ws.on('open', () => {
    console.log('✓ WebSocket connected!');
  });
  
  ws.on('message', (data) => {
    console.log('Message received:', data.toString().substring(0, 100));
  });
  
  ws.on('error', (err) => {
    console.error('✗ WebSocket error:', err.message);
  });
  
  ws.on('close', (code, reason) => {
    console.log('WebSocket closed:', code, reason.toString());
  });
  
  // Also test the local terminal WebSocket
  setTimeout(() => {
    console.log('\nTesting local terminal WebSocket...');
    
    const localWsUrl = 'wss://code-on-the-phone.onrender.com/ws/terminal';
    const localWs = new WebSocket(localWsUrl, {
      headers: {
        'Cookie': sessionCookie
      }
    });
    
    localWs.on('open', () => {
      console.log('✓ Local terminal WebSocket connected!');
      
      // Send a test command
      localWs.send(JSON.stringify({
        type: 'input',
        data: 'echo "WebSocket test successful"\n'
      }));
    });
    
    localWs.on('message', (data) => {
      try {
        const msg = JSON.parse(data);
        if (msg.type === 'output') {
          console.log('Command output:', msg.data);
        } else {
          console.log('Message:', msg);
        }
      } catch (e) {
        console.log('Raw message:', data.toString());
      }
    });
    
    localWs.on('error', (err) => {
      console.error('✗ Local WebSocket error:', err.message);
    });
    
    localWs.on('close', (code, reason) => {
      console.log('Local WebSocket closed:', code, reason.toString());
      process.exit(0);
    });
  }, 2000);
}

testWebSocketProxy().catch(console.error);