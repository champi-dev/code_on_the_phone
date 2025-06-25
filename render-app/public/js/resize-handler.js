// Terminal resize handler with debouncing and optimization
class ResizeHandler {
  constructor(terminal, fitAddon, onResize) {
    this.terminal = terminal;
    this.fitAddon = fitAddon;
    this.onResize = onResize;
    this.resizeTimeout = null;
    this.lastCols = terminal.cols;
    this.lastRows = terminal.rows;
    this.resizeDelay = 300; // ms
    this.immediateResize = true; // First resize is immediate
    
    // Bind resize handler
    this.handleResize = this.handleResize.bind(this);
    
    // Observe terminal container size changes
    if (window.ResizeObserver) {
      this.resizeObserver = new ResizeObserver(this.handleResize);
      const terminalElement = terminal.element || document.getElementById('terminal');
      if (terminalElement) {
        this.resizeObserver.observe(terminalElement);
      }
    }
    
    // Also listen to window resize as fallback
    window.addEventListener('resize', this.handleResize);
  }
  
  handleResize() {
    // Clear existing timeout
    if (this.resizeTimeout) {
      clearTimeout(this.resizeTimeout);
    }
    
    // Immediate resize for first call or significant changes
    if (this.immediateResize) {
      this.performResize();
      this.immediateResize = false;
    }
    
    // Debounced resize
    this.resizeTimeout = setTimeout(() => {
      this.performResize();
      this.immediateResize = true; // Reset for next resize sequence
    }, this.resizeDelay);
  }
  
  performResize() {
    try {
      // Fit terminal to container
      this.fitAddon.fit();
      
      // Check if dimensions actually changed
      const newCols = this.terminal.cols;
      const newRows = this.terminal.rows;
      
      if (newCols !== this.lastCols || newRows !== this.lastRows) {
        this.lastCols = newCols;
        this.lastRows = newRows;
        
        // Call resize callback
        if (this.onResize) {
          this.onResize(newCols, newRows);
        }
        
        console.log(`Terminal resized to ${newCols}x${newRows}`);
      }
    } catch (err) {
      console.error('Error during terminal resize:', err);
    }
  }
  
  // Force immediate resize
  resizeNow() {
    if (this.resizeTimeout) {
      clearTimeout(this.resizeTimeout);
    }
    this.performResize();
  }
  
  // Update resize delay
  setResizeDelay(delay) {
    this.resizeDelay = Math.max(50, Math.min(1000, delay));
  }
  
  // Get current dimensions
  getDimensions() {
    return {
      cols: this.terminal.cols,
      rows: this.terminal.rows,
      width: this.terminal.element ? this.terminal.element.offsetWidth : 0,
      height: this.terminal.element ? this.terminal.element.offsetHeight : 0
    };
  }
  
  // Clean up
  destroy() {
    if (this.resizeTimeout) {
      clearTimeout(this.resizeTimeout);
    }
    
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
    }
    
    window.removeEventListener('resize', this.handleResize);
  }
}

// Auto-fit terminal with aspect ratio preservation
class SmartResizeHandler extends ResizeHandler {
  constructor(terminal, fitAddon, onResize, options = {}) {
    super(terminal, fitAddon, onResize);
    
    this.minCols = options.minCols || 40;
    this.maxCols = options.maxCols || 200;
    this.minRows = options.minRows || 10;
    this.maxRows = options.maxRows || 80;
    this.maintainAspectRatio = options.maintainAspectRatio || false;
    this.targetAspectRatio = options.targetAspectRatio || (16 / 9);
  }
  
  performResize() {
    try {
      // Get container dimensions
      const container = this.terminal.element || document.getElementById('terminal');
      if (!container) return;
      
      const containerWidth = container.offsetWidth;
      const containerHeight = container.offsetHeight;
      
      if (this.maintainAspectRatio) {
        // Calculate dimensions maintaining aspect ratio
        const currentRatio = containerWidth / containerHeight;
        
        if (currentRatio > this.targetAspectRatio) {
          // Container is wider than target ratio
          const targetWidth = containerHeight * this.targetAspectRatio;
          container.style.width = `${targetWidth}px`;
          container.style.margin = '0 auto';
        } else {
          // Container is taller than target ratio
          const targetHeight = containerWidth / this.targetAspectRatio;
          container.style.height = `${targetHeight}px`;
        }
      }
      
      // Fit terminal
      this.fitAddon.fit();
      
      // Clamp dimensions
      let cols = this.terminal.cols;
      let rows = this.terminal.rows;
      
      cols = Math.max(this.minCols, Math.min(this.maxCols, cols));
      rows = Math.max(this.minRows, Math.min(this.maxRows, rows));
      
      // Apply clamped dimensions if needed
      if (cols !== this.terminal.cols || rows !== this.terminal.rows) {
        this.terminal.resize(cols, rows);
      }
      
      // Continue with normal resize handling
      super.performResize();
      
    } catch (err) {
      console.error('Error in smart resize:', err);
    }
  }
}

// Export for use
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { ResizeHandler, SmartResizeHandler };
}