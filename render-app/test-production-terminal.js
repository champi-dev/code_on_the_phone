const https = require('https');

async function testProductionTerminal() {
  const password = 'cloudterm123'; // The password from your test
  
  // Step 1: Login
  console.log('1. Testing login...');
  
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
  
  const sessionCookie = await new Promise((resolve, reject) => {
    const req = https.request(loginOptions, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log('Login response:', res.statusCode, data);
        
        if (res.statusCode === 200) {
          // Extract session cookie
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
  
  if (!sessionCookie) {
    console.error('Failed to login');
    return;
  }
  
  console.log('Got session:', sessionCookie);
  
  // Step 2: Get terminal config
  console.log('\n2. Getting terminal config...');
  
  const configOptions = {
    hostname: 'code-on-the-phone.onrender.com',
    path: '/api/terminal-config',
    method: 'GET',
    headers: {
      'Cookie': sessionCookie
    }
  };
  
  const config = await new Promise((resolve) => {
    https.get(configOptions, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log('Terminal config:', res.statusCode, data);
        if (res.statusCode === 200) {
          resolve(JSON.parse(data));
        } else {
          resolve(null);
        }
      });
    }).on('error', (err) => {
      console.error('Config error:', err);
      resolve(null);
    });
  });
  
  if (!config) {
    console.error('Failed to get config');
    return;
  }
  
  // Step 3: Test terminal health
  console.log('\n3. Testing terminal health...');
  
  const healthOptions = {
    hostname: 'code-on-the-phone.onrender.com',
    path: '/api/terminal-health',
    method: 'GET',
    headers: {
      'Cookie': sessionCookie
    }
  };
  
  await new Promise((resolve) => {
    https.get(healthOptions, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log('Terminal health:', res.statusCode, data);
        resolve();
      });
    }).on('error', (err) => {
      console.error('Health check error:', err);
      resolve();
    });
  });
  
  // Step 4: Test proxy
  console.log('\n4. Testing terminal proxy...');
  
  const proxyOptions = {
    hostname: 'code-on-the-phone.onrender.com',
    path: '/api/proxy-test',
    method: 'GET',
    headers: {
      'Cookie': sessionCookie
    }
  };
  
  await new Promise((resolve) => {
    https.get(proxyOptions, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log('Proxy test:', res.statusCode, data);
        resolve();
      });
    }).on('error', (err) => {
      console.error('Proxy test error:', err);
      resolve();
    });
  });
  
  // Step 5: Test command execution
  console.log('\n5. Testing command execution...');
  
  const commandData = JSON.stringify({ command: 'echo "Test from API"' });
  
  const execOptions = {
    hostname: 'code-on-the-phone.onrender.com',
    path: '/api/exec',
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': commandData.length,
      'Cookie': sessionCookie
    }
  };
  
  await new Promise((resolve) => {
    const req = https.request(execOptions, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log('Command execution:', res.statusCode, data);
        resolve();
      });
    });
    
    req.on('error', (err) => {
      console.error('Exec error:', err);
      resolve();
    });
    
    req.write(commandData);
    req.end();
  });
}

testProductionTerminal().catch(console.error);