// Enhanced terminal handler using node-pty for proper PTY support
const os = require('os');
const pty = require('node-pty');

class TerminalManager {
  constructor() {
    this.sessions = new Map();
  }

  createSession(sessionId) {
    if (this.sessions.has(sessionId)) {
      return this.sessions.get(sessionId);
    }

    // Determine shell based on OS
    const shell = process.platform === 'win32' ? 'powershell.exe' : 'bash';
    
    // Create PTY instance
    const ptyProcess = pty.spawn(shell, [], {
      name: 'xterm-256color',
      cols: 80,
      rows: 24,
      cwd: process.env.HOME || process.cwd(),
      env: {
        ...process.env,
        TERM: 'xterm-256color',
        COLORTERM: 'truecolor'
      }
    });

    const session = {
      id: sessionId,
      pty: ptyProcess,
      lastActivity: Date.now()
    };

    this.sessions.set(sessionId, session);
    
    // Clean up on exit
    ptyProcess.onExit(() => {
      this.sessions.delete(sessionId);
    });

    return session;
  }

  getSession(sessionId) {
    return this.sessions.get(sessionId);
  }

  resizeSession(sessionId, cols, rows) {
    const session = this.sessions.get(sessionId);
    if (session && session.pty) {
      try {
        session.pty.resize(cols, rows);
        return true;
      } catch (err) {
        console.error(`Failed to resize session ${sessionId}:`, err);
        return false;
      }
    }
    return false;
  }

  writeToSession(sessionId, data) {
    const session = this.sessions.get(sessionId);
    if (session && session.pty) {
      session.lastActivity = Date.now();
      session.pty.write(data);
      return true;
    }
    return false;
  }

  closeSession(sessionId) {
    const session = this.sessions.get(sessionId);
    if (session && session.pty) {
      session.pty.kill();
      this.sessions.delete(sessionId);
    }
  }

  // Clean up idle sessions
  cleanupIdleSessions(maxIdleTime = 30 * 60 * 1000) { // 30 minutes
    const now = Date.now();
    for (const [sessionId, session] of this.sessions) {
      if (now - session.lastActivity > maxIdleTime) {
        console.log(`Cleaning up idle session: ${sessionId}`);
        this.closeSession(sessionId);
      }
    }
  }

  getStats() {
    return {
      activeSessions: this.sessions.size,
      sessions: Array.from(this.sessions.entries()).map(([id, session]) => ({
        id,
        lastActivity: session.lastActivity,
        pid: session.pty.pid
      }))
    };
  }
}

// WebSocket handler using node-pty
function handlePtyWebSocket(ws, req) {
  const sessionId = req.sessionID || `session-${Date.now()}`;
  console.log(`New PTY terminal session: ${sessionId}`);
  
  // Create or get session
  const session = terminalManager.createSession(sessionId);
  
  // Handle PTY output
  const outputHandler = (data) => {
    if (ws.readyState === ws.OPEN) {
      ws.send(JSON.stringify({
        type: 'output',
        data: data.toString()
      }));
    }
  };
  
  session.pty.onData(outputHandler);
  
  // Handle PTY exit
  session.pty.onExit(({ exitCode, signal }) => {
    console.log(`PTY exited with code ${exitCode}, signal ${signal}`);
    if (ws.readyState === ws.OPEN) {
      ws.send(JSON.stringify({
        type: 'exit',
        code: exitCode,
        signal
      }));
      ws.close();
    }
  });
  
  // Send initial connected message
  ws.send(JSON.stringify({
    type: 'connected',
    sessionId,
    pid: session.pty.pid
  }));
  
  // Handle WebSocket messages
  ws.on('message', (message) => {
    try {
      const msg = JSON.parse(message);
      
      switch (msg.type) {
        case 'input':
          terminalManager.writeToSession(sessionId, msg.data);
          break;
          
        case 'resize':
          if (msg.cols && msg.rows) {
            const success = terminalManager.resizeSession(sessionId, msg.cols, msg.rows);
            if (!success) {
              ws.send(JSON.stringify({
                type: 'error',
                message: 'Failed to resize terminal'
              }));
            }
          }
          break;
          
        case 'ping':
          ws.send(JSON.stringify({
            type: 'pong',
            timestamp: Date.now()
          }));
          break;
      }
    } catch (err) {
      console.error('WebSocket message error:', err);
      ws.send(JSON.stringify({
        type: 'error',
        message: 'Invalid message format'
      }));
    }
  });
  
  // Handle WebSocket close
  ws.on('close', () => {
    console.log(`WebSocket closed for session: ${sessionId}`);
    // Don't immediately close the PTY session - it might reconnect
    // The cleanup will happen in cleanupIdleSessions
  });
  
  // Handle errors
  ws.on('error', (err) => {
    console.error(`WebSocket error for session ${sessionId}:`, err);
  });
}

// Create global terminal manager
const terminalManager = new TerminalManager();

// Periodic cleanup of idle sessions
setInterval(() => {
  terminalManager.cleanupIdleSessions();
}, 5 * 60 * 1000); // Every 5 minutes

module.exports = {
  TerminalManager,
  terminalManager,
  handlePtyWebSocket
};