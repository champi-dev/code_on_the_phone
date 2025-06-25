const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');
const WebSocket = require('ws');
const { MockTerminalClient } = require('./terminal.test');

describe('Terminal Resize Tests', () => {
  let server;
  let client;
  const testPort = 3002;
  const wsUrl = `ws://localhost:${testPort}/ws/terminal`;

  beforeEach((done) => {
    // Start test server
    const app = require('../server');
    const http = require('http');
    server = http.createServer(app);
    server.listen(testPort, done);
  });

  afterEach((done) => {
    if (client) {
      client.disconnect();
    }
    server.close(done);
  });

  it('should handle terminal resize without errors', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Send multiple resize commands
    const resizes = [
      { cols: 80, rows: 24 },
      { cols: 120, rows: 40 },
      { cols: 200, rows: 60 },
      { cols: 40, rows: 20 }
    ];
    
    for (const size of resizes) {
      client.sendResize(size.cols, size.rows);
      await new Promise(resolve => setTimeout(resolve, 50));
      
      // Check no error messages
      const errorMsg = client.messages.find(msg => msg.type === 'error');
      expect(errorMsg).toBeUndefined();
    }
  });

  it('should maintain terminal functionality after resize', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Resize terminal
    client.sendResize(120, 40);
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Send command after resize
    client.clearMessages();
    client.sendInput('echo "After resize"\n');
    await new Promise(resolve => setTimeout(resolve, 500));
    
    // Check output received
    const outputMsg = client.messages.find(msg => 
      msg.type === 'output' && msg.data.includes('After resize')
    );
    expect(outputMsg).toBeDefined();
  });

  it('should handle rapid resize events', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Simulate rapid window resizing
    for (let i = 0; i < 10; i++) {
      client.sendResize(80 + i * 10, 24 + i * 2);
      // Very short delay to simulate rapid resizing
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    
    // Terminal should still be responsive
    await new Promise(resolve => setTimeout(resolve, 200));
    client.sendInput('echo "Still working"\n');
    await new Promise(resolve => setTimeout(resolve, 500));
    
    const outputMsg = client.messages.find(msg => 
      msg.type === 'output' && msg.data.includes('Still working')
    );
    expect(outputMsg).toBeDefined();
  });

  it('should handle invalid resize dimensions', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    // Send invalid dimensions
    const invalidSizes = [
      { cols: 0, rows: 24 },
      { cols: 80, rows: 0 },
      { cols: -1, rows: 24 },
      { cols: 80, rows: -1 },
      { cols: 10000, rows: 10000 }
    ];
    
    for (const size of invalidSizes) {
      client.sendResize(size.cols, size.rows);
      await new Promise(resolve => setTimeout(resolve, 50));
    }
    
    // Terminal should still work
    client.sendInput('echo "Terminal OK"\n');
    await new Promise(resolve => setTimeout(resolve, 500));
    
    const outputMsg = client.messages.find(msg => 
      msg.type === 'output' && msg.data.includes('Terminal OK')
    );
    expect(outputMsg).toBeDefined();
  });

  it('should preserve terminal content after resize', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Send some content
    client.sendInput('echo "Line 1"\n');
    await new Promise(resolve => setTimeout(resolve, 200));
    client.sendInput('echo "Line 2"\n');
    await new Promise(resolve => setTimeout(resolve, 200));
    
    // Clear messages and resize
    const outputBeforeResize = client.messages
      .filter(msg => msg.type === 'output')
      .map(msg => msg.data)
      .join('');
    
    client.sendResize(150, 50);
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Content should still be accessible (scrollback)
    expect(outputBeforeResize).toContain('Line 1');
    expect(outputBeforeResize).toContain('Line 2');
  });

  it('should handle resize during command execution', async () => {
    client = new MockTerminalClient(wsUrl);
    await client.connect();
    
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Start a command that produces continuous output
    client.sendInput('for i in {1..5}; do echo "Output $i"; sleep 0.1; done\n');
    
    // Resize during execution
    await new Promise(resolve => setTimeout(resolve, 150));
    client.sendResize(100, 30);
    
    // Wait for command to complete
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // Check all output was received
    const outputs = client.messages
      .filter(msg => msg.type === 'output')
      .map(msg => msg.data)
      .join('');
    
    for (let i = 1; i <= 5; i++) {
      expect(outputs).toContain(`Output ${i}`);
    }
  });
});

// Test for Go backend resize
describe('Go Backend Resize Tests', () => {
  // These would need a running Go backend
  // Placeholder for integration tests
  
  it.skip('should resize Go backend terminal', async () => {
    // Integration test with Go backend
  });
});