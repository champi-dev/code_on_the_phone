// Terminal client with WebSocket connection for real command execution
class TerminalClient {
  constructor(term, options = {}) {
    this.term = term;
    this.ws = null;
    this.connected = false;
    this.commandBuffer = '';
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 10;
    this.reconnectDelay = 1000; // Start with 1 second
    this.maxReconnectDelay = 30000; // Max 30 seconds
    this.pingInterval = null;
    this.pongTimeout = null;
    this.lastActivity = Date.now();
    this.options = {
      wsUrl: options.wsUrl || `${window.location.protocol === 'https:' ? 'wss:' : 'ws:'}//${window.location.host}/ws/terminal`,
      reconnect: options.reconnect !== false,
      heartbeatInterval: options.heartbeatInterval || 30000,
      ...options
    };
  }

  connect() {
    if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN)) {
      console.log('WebSocket already connected or connecting');
      return;
    }
    
    console.log(`Connecting to WebSocket: ${this.options.wsUrl} (attempt ${this.reconnectAttempts + 1})`);
    
    try {
      this.ws = new WebSocket(this.options.wsUrl);
    } catch (err) {
      console.error('Failed to create WebSocket:', err);
      this.handleReconnect();
      return;
    }
    
    // Set connection timeout
    const connectTimeout = setTimeout(() => {
      if (this.ws && this.ws.readyState === WebSocket.CONNECTING) {
        console.log('WebSocket connection timeout');
        this.ws.close();
        this.handleReconnect();
      }
    }, 10000); // 10 second timeout
    
    this.ws.onopen = () => {
      clearTimeout(connectTimeout);
      console.log('WebSocket connected');
      this.connected = true;
      this.reconnectAttempts = 0;
      this.reconnectDelay = 1000;
      this.lastActivity = Date.now();
      
      // Clear any error messages
      this.term.write('\r\x1b[K'); // Clear current line
      this.term.writeln('\x1b[32mConnected to terminal server\x1b[0m\r\n');
      
      // Start heartbeat
      this.startHeartbeat();
    };
    
    this.ws.onmessage = (event) => {
      this.lastActivity = Date.now();
      
      try {
        const msg = JSON.parse(event.data);
        
        switch (msg.type) {
          case 'output':
            // Write output to terminal
            this.term.write(msg.data);
            break;
            
          case 'connected':
            // Initial connection established
            console.log('Terminal ready, shell PID:', msg.shellPid);
            break;
            
          case 'exit':
            this.term.writeln(`\r\n\x1b[33mProcess exited with code ${msg.code}${msg.signal ? ` (signal: ${msg.signal})` : ''}\x1b[0m`);
            // Don't disconnect - server will restart shell
            break;
            
          case 'error':
            this.term.writeln(`\r\n\x1b[31mError: ${msg.message}\x1b[0m`);
            break;
            
          case 'pong':
            // Heartbeat response
            this.handlePong();
            break;
            
          default:
            console.warn('Unknown message type:', msg.type);
        }
      } catch (err) {
        console.error('Failed to parse WebSocket message:', err);
        // Try to display raw message if JSON parsing fails
        if (typeof event.data === 'string' && event.data.length < 1000) {
          this.term.write(event.data);
        }
      }
    };
    
    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error);
      clearTimeout(connectTimeout);
      
      if (this.connected) {
        this.term.writeln('\r\n\x1b[31mConnection error - attempting to reconnect...\x1b[0m');
      }
    };
    
    this.ws.onclose = (event) => {
      clearTimeout(connectTimeout);
      console.log(`WebSocket closed: ${event.code} - ${event.reason || 'No reason'}`);
      this.connected = false;
      this.stopHeartbeat();
      
      // Determine close reason
      let closeMessage = '\r\n\x1b[33mDisconnected from server';
      if (event.code === 1000) {
        closeMessage += ' (normal closure)';
      } else if (event.code === 1001) {
        closeMessage += ' (endpoint going away)';
      } else if (event.code === 1006) {
        closeMessage += ' (connection lost)';
      } else if (event.reason) {
        closeMessage += ` (${event.reason})`;
      }
      closeMessage += '\x1b[0m';
      
      this.term.writeln(closeMessage);
      
      // Handle reconnection
      this.handleReconnect();
    };
    
    // Handle terminal input
    this.term.onData((data) => {
      if (this.connected && this.ws.readyState === WebSocket.OPEN) {
        // Send input to server
        this.ws.send(JSON.stringify({
          type: 'input',
          data: data
        }));
        
        // Track command for 3D effects
        this.trackCommand(data);
      }
    });
  }
  
  trackCommand(data) {
    if (data === '\r' || data === '\n') {
      // Command executed - trigger 3D effect
      if (window.parent !== window) {
        window.parent.postMessage({
          type: 'command',
          command: this.commandBuffer
        }, '*');
      }
      this.commandBuffer = '';
    } else if (data === '\x7f' || data === '\x08') {
      // Backspace or Delete
      this.commandBuffer = this.commandBuffer.slice(0, -1);
    } else if (data === '\x03') {
      // Ctrl+C - clear buffer
      this.commandBuffer = '';
    } else if (data.charCodeAt(0) >= 32 && data.charCodeAt(0) < 127) {
      // Printable ASCII character
      this.commandBuffer += data;
    }
  }
  
  disconnect() {
    this.options.reconnect = false; // Disable auto-reconnect
    this.stopHeartbeat();
    
    if (this.ws) {
      this.ws.close(1000, 'User initiated disconnect');
      this.ws = null;
    }
    this.connected = false;
  }
  
  sendCommand(command) {
    if (this.connected && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({
        type: 'input',
        data: command + '\n'
      }));
    }
  }
  
  resize(cols, rows) {
    if (this.connected && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({
        type: 'resize',
        cols: cols,
        rows: rows
      }));
    }
  }
  
  // Reconnection logic with exponential backoff
  handleReconnect() {
    if (!this.options.reconnect) {
      return;
    }
    
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      this.term.writeln('\r\n\x1b[31mMaximum reconnection attempts reached. Please refresh the page.\x1b[0m');
      return;
    }
    
    this.reconnectAttempts++;
    const delay = Math.min(this.reconnectDelay * Math.pow(1.5, this.reconnectAttempts - 1), this.maxReconnectDelay);
    
    this.term.writeln(`\r\n\x1b[33mReconnecting in ${Math.round(delay / 1000)} seconds... (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})\x1b[0m`);
    
    setTimeout(() => {
      if (!this.connected && this.options.reconnect) {
        this.connect();
      }
    }, delay);
  }
  
  // Heartbeat mechanism to detect stale connections
  startHeartbeat() {
    this.stopHeartbeat();
    
    this.pingInterval = setInterval(() => {
      if (this.connected && this.ws.readyState === WebSocket.OPEN) {
        // Send ping
        try {
          this.ws.send(JSON.stringify({ type: 'ping', timestamp: Date.now() }));
          
          // Set pong timeout
          this.pongTimeout = setTimeout(() => {
            console.log('Pong timeout - connection may be stale');
            this.ws.close(4001, 'Ping timeout');
          }, 5000); // 5 second pong timeout
        } catch (err) {
          console.error('Failed to send ping:', err);
        }
      }
    }, this.options.heartbeatInterval);
  }
  
  stopHeartbeat() {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
    if (this.pongTimeout) {
      clearTimeout(this.pongTimeout);
      this.pongTimeout = null;
    }
  }
  
  handlePong() {
    if (this.pongTimeout) {
      clearTimeout(this.pongTimeout);
      this.pongTimeout = null;
    }
  }
  
  // Get connection status
  getStatus() {
    return {
      connected: this.connected,
      reconnectAttempts: this.reconnectAttempts,
      lastActivity: this.lastActivity,
      wsState: this.ws ? this.ws.readyState : null
    };
  }
}