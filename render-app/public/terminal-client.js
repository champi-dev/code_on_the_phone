// Terminal client with WebSocket connection for real command execution
class TerminalClient {
  constructor(term, options = {}) {
    this.term = term;
    this.ws = null;
    this.connected = false;
    this.commandBuffer = '';
    this.options = {
      wsUrl: options.wsUrl || `${window.location.protocol === 'https:' ? 'wss:' : 'ws:'}//${window.location.host}/ws/terminal`,
      ...options
    };
  }

  connect() {
    console.log('Connecting to WebSocket:', this.options.wsUrl);
    
    this.ws = new WebSocket(this.options.wsUrl);
    
    this.ws.onopen = () => {
      console.log('WebSocket connected');
      this.connected = true;
      this.term.writeln('Connected to terminal server...\r\n');
    };
    
    this.ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        
        switch (msg.type) {
          case 'output':
            // Write output to terminal
            this.term.write(msg.data);
            break;
            
          case 'connected':
            // Initial connection established
            console.log('Terminal ready');
            break;
            
          case 'exit':
            this.term.writeln(`\r\nProcess exited with code ${msg.code}`);
            this.disconnect();
            break;
        }
      } catch (err) {
        console.error('Failed to parse WebSocket message:', err);
      }
    };
    
    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error);
      this.term.writeln('\r\n\x1b[31mConnection error\x1b[0m');
    };
    
    this.ws.onclose = () => {
      console.log('WebSocket closed');
      this.connected = false;
      this.term.writeln('\r\n\x1b[33mDisconnected from server\x1b[0m');
      
      // Attempt to reconnect after 3 seconds
      setTimeout(() => {
        if (!this.connected) {
          this.term.writeln('\r\nReconnecting...');
          this.connect();
        }
      }, 3000);
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
    } else if (data === '\x7f') {
      // Backspace
      this.commandBuffer = this.commandBuffer.slice(0, -1);
    } else if (data.charCodeAt(0) >= 32) {
      // Regular character
      this.commandBuffer += data;
    }
  }
  
  disconnect() {
    if (this.ws) {
      this.ws.close();
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
}