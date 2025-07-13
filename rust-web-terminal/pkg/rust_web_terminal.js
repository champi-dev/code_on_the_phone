// Simulated Rust Web Terminal for testing
export class WebTerminal {
  constructor(containerId) {
    this.containerId = containerId;
    this.connected = false;
    console.log('WebTerminal initialized for container:', containerId);
  }

  async connect(host, password) {
    console.log(`Connecting to ${host} with password ${password}`);
    
    // Simulate connection delay
    await new Promise(resolve => setTimeout(resolve, 1500));
    
    // For demo purposes, simulate a successful connection
    this.connected = true;
    console.log('Connected successfully!');
    
    // Show terminal content
    const container = document.getElementById(this.containerId);
    if (container) {
      // Create a simple terminal display
      const terminalContent = `
        <div style="background: #000; color: #0f0; padding: 20px; font-family: monospace; height: 100%; overflow-y: auto;">
          <div>Rust Web Terminal v0.1.0 (Demo Mode)</div>
          <div>Connected to ${host}</div>
          <div>---</div>
          <div>$ ls -la</div>
          <div>total 48</div>
          <div>drwxr-xr-x  5 user user 4096 Jul 13 10:00 .</div>
          <div>drwxr-xr-x 10 user user 4096 Jul 13 09:00 ..</div>
          <div>-rw-r--r--  1 user user  220 Jul 13 08:00 .bashrc</div>
          <div>drwxr-xr-x  2 user user 4096 Jul 13 09:30 projects</div>
          <div>$ pwd</div>
          <div>/home/user</div>
          <div>$ <span style="animation: blink 1s infinite">█</span></div>
        </div>
      `;
      container.innerHTML = terminalContent;
    }
  }

  disconnect() {
    this.connected = false;
    console.log('Disconnected');
  }

  resize(cols, rows) {
    console.log(`Resizing to ${cols}x${rows}`);
  }
}

// CSS for cursor blink
const style = document.createElement('style');
style.textContent = `
  @keyframes blink {
    0%, 50% { opacity: 1; }
    51%, 100% { opacity: 0; }
  }
`;
document.head.appendChild(style);

export default async function init() {
  console.log('WASM module initialized (demo mode)');
}