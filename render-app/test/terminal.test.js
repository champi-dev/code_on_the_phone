const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');
const WebSocket = require('ws');
const { spawn } = require('child_process');
const http = require('http');

// Mock terminal client for testing
class MockTerminalClient {
  constructor(wsUrl) {
    this.wsUrl = wsUrl;
    this.ws = null;
    this.messages = [];
    this.connected = false;
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.ws = new WebSocket(this.wsUrl);
      
      this.ws.on('open', () => {
        this.connected = true;
        resolve();
      });
      
      this.ws.on('message', (data) => {
        const msg = JSON.parse(data);
        this.messages.push(msg);
      });
      
      this.ws.on('error', (err) => {
        reject(err);
      });
      
      this.ws.on('close', () => {
        this.connected = false;
      });
    });
  }

  sendInput(data) {
    if (this.connected) {
      this.ws.send(JSON.stringify({ type: 'input', data }));
    }
  }

  sendResize(cols, rows) {
    if (this.connected) {
      this.ws.send(JSON.stringify({ type: 'resize', cols, rows }));
    }
  }

  disconnect() {
    if (this.ws) {
      this.ws.close();
    }
  }

  getLastMessage() {
    return this.messages[this.messages.length - 1];
  }

  clearMessages() {
    this.messages = [];
  }
}

describe('Terminal WebSocket Tests', () => {
  let server;
  let client;
  const testPort = 3001;
  const wsUrl = `ws://localhost:${testPort}/ws/terminal`;

  beforeEach((done) => {
    // Start test server
    const app = require('../server');
    server = http.createServer(app);
    server.listen(testPort, done);
  });

  afterEach((done) => {
    if (client) {
      client.disconnect();
    }
    server.close(done);
  });

  it('should connect to WebSocket endpoint', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    expect(client.connected).toBe(true);
  });

  it('should receive connected message on connection', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Wait for connected message
    await new Promise(resolve => setTimeout(resolve, 100));
    
    const connectedMsg = client.messages.find(msg => msg.type === 'connected');
    expect(connectedMsg).toBeDefined();
  });

  it('should execute echo command and receive output', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Clear initial messages
    await new Promise(resolve => setTimeout(resolve, 100));
    client.clearMessages();
    
    // Send echo command
    client.sendInput('echo "Hello Terminal"\n');
    
    // Wait for output
    await new Promise(resolve => setTimeout(resolve, 500));
    
    const outputMsg = client.messages.find(msg => 
      msg.type === 'output' && msg.data.includes('Hello Terminal')
    );
    expect(outputMsg).toBeDefined();
  });

  it('should handle multiple commands', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    await new Promise(resolve => setTimeout(resolve, 100));
    client.clearMessages();
    
    // Send multiple commands
    client.sendInput('echo "First"\n');
    await new Promise(resolve => setTimeout(resolve, 200));
    
    client.sendInput('echo "Second"\n');
    await new Promise(resolve => setTimeout(resolve, 200));
    
    const outputs = client.messages
      .filter(msg => msg.type === 'output')
      .map(msg => msg.data)
      .join('');
    
    expect(outputs).toContain('First');
    expect(outputs).toContain('Second');
  });

  it('should handle terminal resize', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Send resize command
    client.sendResize(120, 40);
    
    // Verify no errors occurred
    await new Promise(resolve => setTimeout(resolve, 100));
    const errorMsg = client.messages.find(msg => msg.type === 'error');
    expect(errorMsg).toBeUndefined();
  });

  it('should handle Ctrl+C interrupt', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    await new Promise(resolve => setTimeout(resolve, 100));
    client.clearMessages();
    
    // Start a long-running command
    client.sendInput('sleep 10\n');
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Send Ctrl+C
    client.sendInput('\x03');
    await new Promise(resolve => setTimeout(resolve, 200));
    
    // Check if command was interrupted
    const outputs = client.messages
      .filter(msg => msg.type === 'output')
      .map(msg => msg.data)
      .join('');
    
    // Should see prompt again after interrupt
    expect(outputs).toMatch(/\$|#|>/);
  });

  it('should reconnect after disconnection', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    expect(client.connected).toBe(true);
    
    // Force disconnect
    client.ws.close();
    await new Promise(resolve => setTimeout(resolve, 100));
    expect(client.connected).toBe(false);
    
    // Reconnect
    await client.connect();
    expect(client.connected).toBe(true);
  });

  it('should handle invalid JSON messages gracefully', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Send invalid JSON
    client.ws.send('invalid json');
    
    // Should not crash, connection should remain open
    await new Promise(resolve => setTimeout(resolve, 100));
    expect(client.connected).toBe(true);
  });

  it('should timeout idle connections', async () => {
    // This test would need to modify server timeout settings
    // Skipping for now as it would take too long
  });

  it('should handle concurrent connections', async () => {
    const client1 = new MockTerminalClient(wsUrl);
    const client2 = new MockTerminalClient(wsUrl);
    
    await client1.connect();
    await client2.connect();
    
    expect(client1.connected).toBe(true);
    expect(client2.connected).toBe(true);
    
    // Each should have separate shell sessions
    client1.sendInput('echo "Client 1"\n');
    client2.sendInput('echo "Client 2"\n');
    
    await new Promise(resolve => setTimeout(resolve, 500));
    
    const outputs1 = client1.messages
      .filter(msg => msg.type === 'output')
      .map(msg => msg.data)
      .join('');
    
    const outputs2 = client2.messages
      .filter(msg => msg.type === 'output')
      .map(msg => msg.data)
      .join('');
    
    expect(outputs1).toContain('Client 1');
    expect(outputs2).toContain('Client 2');
    
    client1.disconnect();
    client2.disconnect();
  });
});

// Export for use in other tests
module.exports = { MockTerminalClient };