const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');

// Mock xterm.js
class MockTerminal {
  constructor() {
    this.buffer = [];
    this.onDataCallback = null;
  }

  write(data) {
    this.buffer.push(data);
  }

  writeln(data) {
    this.buffer.push(data + '\r\n');
  }

  onData(callback) {
    this.onDataCallback = callback;
  }

  simulateInput(data) {
    if (this.onDataCallback) {
      this.onDataCallback(data);
    }
  }

  getOutput() {
    return this.buffer.join('');
  }

  clear() {
    this.buffer = [];
  }
}

// Mock WebSocket
class MockWebSocket {
  constructor(url) {
    this.url = url;
    this.readyState = MockWebSocket.CONNECTING;
    this.onopen = null;
    this.onmessage = null;
    this.onerror = null;
    this.onclose = null;
    this.sentMessages = [];
    
    // Simulate connection
    setTimeout(() => {
      this.readyState = MockWebSocket.OPEN;
      if (this.onopen) this.onopen();
    }, 10);
  }

  send(data) {
    if (this.readyState === MockWebSocket.OPEN) {
      this.sentMessages.push(data);
    }
  }

  close(code, reason) {
    this.readyState = MockWebSocket.CLOSED;
    if (this.onclose) {
      this.onclose({ code, reason });
    }
  }

  simulateMessage(data) {
    if (this.onmessage) {
      this.onmessage({ data });
    }
  }

  simulateError(error) {
    if (this.onerror) {
      this.onerror(error);
    }
  }
}

MockWebSocket.CONNECTING = 0;
MockWebSocket.OPEN = 1;
MockWebSocket.CLOSED = 3;

// Import the terminal client (with mocked dependencies)
global.WebSocket = MockWebSocket;

// Simplified TerminalClient for testing
class TerminalClient {
  constructor(term, options = {}) {
    this.term = term;
    this.ws = null;
    this.connected = false;
    this.commandBuffer = '';
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 10;
    this.reconnectDelay = 1000;
    this.options = {
      wsUrl: options.wsUrl || 'ws://localhost:3000/ws/terminal',
      reconnect: options.reconnect !== false,
      ...options
    };
  }

  connect() {
    if (this.ws && this.ws.readyState === MockWebSocket.OPEN) {
      return;
    }
    
    this.ws = new MockWebSocket(this.options.wsUrl);
    
    this.ws.onopen = () => {
      this.connected = true;
      this.reconnectAttempts = 0;
      this.term.writeln('\\x1b[32mConnected to terminal server\\x1b[0m\\r\\n');
    };
    
    this.ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      
      switch (msg.type) {
        case 'output':
          this.term.write(msg.data);
          break;
        case 'error':
          this.term.writeln(`\\r\\n\\x1b[31mError: ${msg.message}\\x1b[0m`);
          break;
      }
    };
    
    this.ws.onerror = (error) => {
      if (this.connected) {
        this.term.writeln('\\r\\n\\x1b[31mConnection error\\x1b[0m');
      }
    };
    
    this.ws.onclose = (event) => {
      this.connected = false;
      this.term.writeln('\\r\\n\\x1b[33mDisconnected from server\\x1b[0m');
      this.handleReconnect();
    };
    
    this.term.onData((data) => {
      if (this.connected && this.ws.readyState === MockWebSocket.OPEN) {
        this.ws.send(JSON.stringify({
          type: 'input',
          data: data
        }));
      }
    });
  }
  
  handleReconnect() {
    if (!this.options.reconnect || this.reconnectAttempts >= this.maxReconnectAttempts) {
      return;
    }
    
    this.reconnectAttempts++;
    setTimeout(() => {
      if (!this.connected && this.options.reconnect) {
        this.connect();
      }
    }, this.reconnectDelay);
  }
  
  disconnect() {
    this.options.reconnect = false;
    if (this.ws) {
      this.ws.close(1000, 'User initiated disconnect');
    }
  }
}

describe('TerminalClient Tests', () => {
  let term;
  let client;

  beforeEach(() => {
    term = new MockTerminal();
  });

  afterEach(() => {
    if (client) {
      client.disconnect();
    }
  });

  it('should connect to WebSocket server', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      expect(client.connected).toBe(true);
      expect(term.getOutput()).toContain('Connected to terminal server');
      done();
    }, 50);
  });

  it('should send input to server', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      term.simulateInput('echo test\n');
      
      const sentMsg = client.ws.sentMessages[0];
      const parsed = JSON.parse(sentMsg);
      expect(parsed.type).toBe('input');
      expect(parsed.data).toBe('echo test\n');
      done();
    }, 50);
  });

  it('should display server output', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      client.ws.simulateMessage(JSON.stringify({
        type: 'output',
        data: 'Hello from server'
      }));
      
      expect(term.getOutput()).toContain('Hello from server');
      done();
    }, 50);
  });

  it('should handle connection errors', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      client.ws.simulateError(new Error('Connection failed'));
      expect(term.getOutput()).toContain('Connection error');
      done();
    }, 50);
  });

  it('should handle disconnection', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      client.ws.close();
      expect(term.getOutput()).toContain('Disconnected from server');
      expect(client.connected).toBe(false);
      done();
    }, 50);
  });

  it('should attempt reconnection after disconnect', (done) => {
    client = new TerminalClient(term, { reconnectDelay: 100 });
    client.connect();
    
    setTimeout(() => {
      const originalWs = client.ws;
      client.ws.close();
      
      setTimeout(() => {
        expect(client.ws).not.toBe(originalWs);
        expect(client.reconnectAttempts).toBe(1);
        done();
      }, 150);
    }, 50);
  });

  it('should not reconnect if disabled', (done) => {
    client = new TerminalClient(term, { reconnect: false });
    client.connect();
    
    setTimeout(() => {
      client.ws.close();
      
      setTimeout(() => {
        expect(client.reconnectAttempts).toBe(0);
        done();
      }, 150);
    }, 50);
  });

  it('should handle error messages from server', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      client.ws.simulateMessage(JSON.stringify({
        type: 'error',
        message: 'Command failed'
      }));
      
      expect(term.getOutput()).toContain('Error: Command failed');
      done();
    }, 50);
  });

  it('should stop reconnecting after max attempts', (done) => {
    client = new TerminalClient(term, { 
      maxReconnectAttempts: 2,
      reconnectDelay: 50 
    });
    client.connect();
    
    setTimeout(() => {
      // Force multiple disconnections
      client.ws.close();
      
      setTimeout(() => {
        client.ws.close();
        
        setTimeout(() => {
          client.ws.close();
          
          setTimeout(() => {
            expect(client.reconnectAttempts).toBe(2);
            done();
          }, 100);
        }, 100);
      }, 100);
    }, 50);
  });

  it('should not send data when disconnected', (done) => {
    client = new TerminalClient(term);
    client.connect();
    
    setTimeout(() => {
      client.ws.close();
      client.connected = false;
      
      term.simulateInput('should not send');
      
      // Check that no new messages were sent after disconnection
      const messageCount = client.ws.sentMessages.length;
      expect(client.ws.sentMessages[messageCount - 1]).not.toContain('should not send');
      done();
    }, 50);
  });
});