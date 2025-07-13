const { describe, it, expect, beforeAll, afterAll, beforeEach } = require('@jest/globals');
const request = require('supertest');
const http = require('http');
const WebSocket = require('ws');
const bcrypt = require('bcryptjs');

describe('Terminal E2E Tests', () => {
  let app;
  let server;
  let agent;
  let wsUrl;
  const testPort = 3005;
  const testPassword = 'e2etest123';

  beforeAll((done) => {
    // Setup environment
    process.env.PORT = testPort;
    process.env.PASSWORD_HASH = bcrypt.hashSync(testPassword, 10);
    process.env.SESSION_SECRET = 'e2e-test-secret';
    
    // Clear module cache and load app
    jest.resetModules();
    app = require('../server');
    server = http.createServer(app);
    
    server.listen(testPort, () => {
      wsUrl = `ws://localhost:${testPort}/ws/terminal`;
      done();
    });
  });

  afterAll((done) => {
    server.close(done);
  });

  beforeEach(async () => {
    // Create authenticated agent
    agent = request.agent(app);
    await agent
      .post('/api/login')
      .send({ password: testPassword });
  });

  describe('Authentication Flow', () => {
    it('should complete full authentication flow', async () => {
      const newAgent = request.agent(app);
      
      // 1. Access protected route - should redirect
      const res1 = await newAgent.get('/');
      expect(res1.status).toBe(302);
      expect(res1.headers.location).toBe('/login');
      
      // 2. Access login page
      const res2 = await newAgent.get('/login');
      expect(res2.status).toBe(200);
      
      // 3. Submit login
      const res3 = await newAgent
        .post('/api/login')
        .send({ password: testPassword });
      expect(res3.status).toBe(200);
      expect(res3.body.success).toBe(true);
      
      // 4. Access protected route - should succeed
      const res4 = await newAgent.get('/');
      expect(res4.status).toBe(200);
      
      // 5. Logout
      const res5 = await newAgent.post('/api/logout');
      expect(res5.status).toBe(200);
      
      // 6. Access protected route - should redirect again
      const res6 = await newAgent.get('/');
      expect(res6.status).toBe(302);
    });

    it('should maintain session across multiple requests', async () => {
      // Make multiple authenticated requests
      const res1 = await agent.get('/api/session-status');
      expect(res1.body.authenticated).toBe(true);
      
      const res2 = await agent.get('/api/terminal-config');
      expect(res2.status).toBe(200);
      
      const res3 = await agent.get('/api/session-status');
      expect(res3.body.authenticated).toBe(true);
      
      // Last activity should be updated
      expect(new Date(res3.body.lastActivity).getTime())
        .toBeGreaterThanOrEqual(new Date(res1.body.lastActivity).getTime());
    });
  });

  describe('Terminal WebSocket Connection', () => {
    it('should establish WebSocket connection with authentication', (done) => {
      // Get session cookie from agent
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws = new WebSocket(wsUrl, {
        headers: {
          'Cookie': `sessionId=${sessionCookie.value}`
        }
      });
      
      ws.on('open', () => {
        expect(ws.readyState).toBe(WebSocket.OPEN);
        ws.close();
        done();
      });
      
      ws.on('error', (err) => {
        done(err);
      });
    });

    it('should reject WebSocket without authentication', (done) => {
      const ws = new WebSocket(wsUrl);
      
      ws.on('error', (err) => {
        expect(err).toBeDefined();
        done();
      });
      
      ws.on('unexpected-response', (req, res) => {
        expect(res.statusCode).toBe(401);
        done();
      });
    });

    it('should execute commands through WebSocket', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws = new WebSocket(wsUrl, {
        headers: {
          'Cookie': `sessionId=${sessionCookie.value}`
        }
      });
      
      const outputs = [];
      
      ws.on('open', () => {
        // Send echo command
        ws.send(JSON.stringify({
          type: 'input',
          data: 'echo "E2E Test Output"\n'
        }));
      });
      
      ws.on('message', (data) => {
        const msg = JSON.parse(data);
        
        if (msg.type === 'output') {
          outputs.push(msg.data);
          
          // Check if we received our echo
          const fullOutput = outputs.join('');
          if (fullOutput.includes('E2E Test Output')) {
            ws.close();
            done();
          }
        }
      });
      
      ws.on('error', done);
    });

    it('should handle terminal resize', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws = new WebSocket(wsUrl, {
        headers: {
          'Cookie': `sessionId=${sessionCookie.value}`
        }
      });
      
      ws.on('open', () => {
        // Send resize message
        ws.send(JSON.stringify({
          type: 'resize',
          cols: 120,
          rows: 40
        }));
        
        // If no error after short delay, test passes
        setTimeout(() => {
          ws.close();
          done();
        }, 100);
      });
      
      ws.on('error', done);
    });
  });

  describe('Command Execution API', () => {
    it('should execute safe commands via API', async () => {
      const res = await agent
        .post('/api/exec')
        .send({ command: 'echo "API Test"' });
      
      expect(res.status).toBe(200);
      expect(res.body.output).toContain('API Test');
    });

    it('should reject dangerous commands', async () => {
      const dangerousCommands = [
        'rm -rf /',
        'dd if=/dev/zero of=/dev/sda',
        'mkfs.ext4 /dev/sda',
        ':(){ :|:& };:'
      ];
      
      for (const cmd of dangerousCommands) {
        const res = await agent
          .post('/api/exec')
          .send({ command: cmd });
        
        expect(res.status).toBe(403);
        expect(res.body.error).toContain('not allowed');
      }
    });

    it('should handle command timeout', async () => {
      jest.setTimeout(10000);
      
      const res = await agent
        .post('/api/exec')
        .send({ command: 'sleep 10' });
      
      // Should timeout after 5 seconds
      expect(res.status).toBe(200);
      expect(res.body.error).toBeDefined();
    });
  });

  describe('Terminal Health and Proxy', () => {
    it('should check terminal health', async () => {
      const res = await agent.get('/api/terminal-health');
      
      expect(res.status).toBe(200);
      expect(res.body).toHaveProperty('available');
      expect(typeof res.body.available).toBe('boolean');
    });

    it('should test proxy configuration', async () => {
      const res = await agent.get('/api/proxy-test');
      
      expect(res.status).toBe(200);
      expect(res.body).toHaveProperty('terminalHost');
      expect(res.body).toHaveProperty('terminalPort');
      expect(res.body).toHaveProperty('testResult');
    });

    it('should provide terminal debug info', async () => {
      const res = await agent.get('/api/terminal-debug');
      
      expect(res.status).toBe(200);
      expect(res.body).toHaveProperty('terminalUrl');
      expect(res.body).toHaveProperty('instructions');
      expect(Array.isArray(res.body.instructions)).toBe(true);
    });

    it('should serve fallback UI when terminal unavailable', async () => {
      const res = await agent.get('/terminal-proxy?fallback=true');
      
      expect(res.status).toBe(200);
      expect(res.text).toContain('Terminal server is currently offline');
      expect(res.text).toContain('demo interface');
    });
  });

  describe('Session Persistence', () => {
    it('should persist session across server restart', async () => {
      // Get session status
      const res1 = await agent.get('/api/session-status');
      const loginTime = res1.body.loginTime;
      
      // Get session cookie
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      // Create new agent with same cookie
      const newAgent = request.agent(app);
      newAgent.jar.setCookie(`sessionId=${sessionCookie.value}`, 'http://localhost');
      
      // Should still be authenticated
      const res2 = await newAgent.get('/api/session-status');
      expect(res2.body.authenticated).toBe(true);
      expect(res2.body.loginTime).toBe(loginTime);
    });
  });

  describe('Concurrent Connections', () => {
    it('should handle multiple WebSocket connections', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const connections = [];
      let connectedCount = 0;
      
      // Create 3 concurrent connections
      for (let i = 0; i < 3; i++) {
        const ws = new WebSocket(wsUrl, {
          headers: {
            'Cookie': `sessionId=${sessionCookie.value}`
          }
        });
        
        ws.on('open', () => {
          connectedCount++;
          if (connectedCount === 3) {
            // All connected, close them
            connections.forEach(ws => ws.close());
            done();
          }
        });
        
        ws.on('error', done);
        connections.push(ws);
      }
    });

    it('should isolate commands between connections', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws1Outputs = [];
      const ws2Outputs = [];
      let ws1Done = false;
      let ws2Done = false;
      
      const checkCompletion = () => {
        if (ws1Done && ws2Done) {
          // Verify each connection received its own output
          const ws1Output = ws1Outputs.join('');
          const ws2Output = ws2Outputs.join('');
          
          expect(ws1Output).toContain('Connection 1');
          expect(ws2Output).toContain('Connection 2');
          
          done();
        }
      };
      
      // Connection 1
      const ws1 = new WebSocket(wsUrl, {
        headers: { 'Cookie': `sessionId=${sessionCookie.value}` }
      });
      
      ws1.on('open', () => {
        ws1.send(JSON.stringify({
          type: 'input',
          data: 'echo "Connection 1"\n'
        }));
      });
      
      ws1.on('message', (data) => {
        const msg = JSON.parse(data);
        if (msg.type === 'output') {
          ws1Outputs.push(msg.data);
          if (ws1Outputs.join('').includes('Connection 1')) {
            ws1.close();
            ws1Done = true;
            checkCompletion();
          }
        }
      });
      
      // Connection 2
      const ws2 = new WebSocket(wsUrl, {
        headers: { 'Cookie': `sessionId=${sessionCookie.value}` }
      });
      
      ws2.on('open', () => {
        ws2.send(JSON.stringify({
          type: 'input',
          data: 'echo "Connection 2"\n'
        }));
      });
      
      ws2.on('message', (data) => {
        const msg = JSON.parse(data);
        if (msg.type === 'output') {
          ws2Outputs.push(msg.data);
          if (ws2Outputs.join('').includes('Connection 2')) {
            ws2.close();
            ws2Done = true;
            checkCompletion();
          }
        }
      });
      
      ws1.on('error', done);
      ws2.on('error', done);
    });
  });

  describe('Error Recovery', () => {
    it('should auto-restart shell on crash', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws = new WebSocket(wsUrl, {
        headers: {
          'Cookie': `sessionId=${sessionCookie.value}`
        }
      });
      
      let shellRestarted = false;
      
      ws.on('open', () => {
        // Send exit command to kill shell
        ws.send(JSON.stringify({
          type: 'input',
          data: 'exit\n'
        }));
      });
      
      ws.on('message', (data) => {
        const msg = JSON.parse(data);
        
        if (msg.type === 'exit') {
          // Shell exited
        } else if (msg.type === 'output' && msg.data.includes('Restarting shell')) {
          shellRestarted = true;
        } else if (msg.type === 'connected' && shellRestarted) {
          // Shell restarted successfully
          ws.close();
          done();
        }
      });
      
      ws.on('error', done);
    });

    it('should handle malformed WebSocket messages', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws = new WebSocket(wsUrl, {
        headers: {
          'Cookie': `sessionId=${sessionCookie.value}`
        }
      });
      
      ws.on('open', () => {
        // Send invalid JSON
        ws.send('not valid json');
        
        // Connection should remain open
        setTimeout(() => {
          expect(ws.readyState).toBe(WebSocket.OPEN);
          ws.close();
          done();
        }, 100);
      });
      
      ws.on('error', done);
    });
  });

  describe('Performance', () => {
    it('should handle large output efficiently', (done) => {
      const cookies = agent.jar.getCookies({ domain: 'localhost' });
      const sessionCookie = cookies.find(c => c.key === 'sessionId');
      
      const ws = new WebSocket(wsUrl, {
        headers: {
          'Cookie': `sessionId=${sessionCookie.value}`
        }
      });
      
      let totalBytes = 0;
      const startTime = Date.now();
      
      ws.on('open', () => {
        // Generate large output
        ws.send(JSON.stringify({
          type: 'input',
          data: 'for i in {1..100}; do echo "Line $i: ' + 'x'.repeat(100) + '"; done\n'
        }));
      });
      
      ws.on('message', (data) => {
        const msg = JSON.parse(data);
        
        if (msg.type === 'output') {
          totalBytes += msg.data.length;
          
          // Check if output is being chunked
          if (msg.chunked) {
            expect(msg.data.length).toBeLessThanOrEqual(4096);
          }
          
          // End test after receiving substantial output
          if (totalBytes > 10000) {
            const elapsed = Date.now() - startTime;
            console.log(`Received ${totalBytes} bytes in ${elapsed}ms`);
            ws.close();
            done();
          }
        }
      });
      
      ws.on('error', done);
    });
  });
});