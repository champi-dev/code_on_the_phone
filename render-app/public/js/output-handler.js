// Output handler with rate limiting for large outputs
class OutputHandler {
  constructor(terminal) {
    this.terminal = terminal;
    this.outputQueue = [];
    this.processing = false;
    this.bytesReceived = 0;
    this.startTime = Date.now();
    this.maxBytesPerSecond = 1024 * 1024; // 1MB/s rate limit
    this.chunkDelay = 10; // ms between chunks
  }

  handleOutput(data) {
    // Add to queue
    this.outputQueue.push(data);
    this.bytesReceived += data.length;
    
    // Process queue if not already processing
    if (!this.processing) {
      this.processQueue();
    }
  }

  async processQueue() {
    this.processing = true;
    
    while (this.outputQueue.length > 0) {
      const data = this.outputQueue.shift();
      
      // Write to terminal
      this.terminal.write(data);
      
      // Rate limiting for large outputs
      const elapsed = Date.now() - this.startTime;
      const expectedTime = (this.bytesReceived / this.maxBytesPerSecond) * 1000;
      
      if (elapsed < expectedTime) {
        // We're going too fast, add delay
        await new Promise(resolve => setTimeout(resolve, this.chunkDelay));
      }
      
      // Reset rate limiting every second
      if (elapsed > 1000) {
        this.bytesReceived = 0;
        this.startTime = Date.now();
      }
    }
    
    this.processing = false;
  }

  clear() {
    this.outputQueue = [];
    this.bytesReceived = 0;
    this.startTime = Date.now();
  }

  getStats() {
    const elapsed = Date.now() - this.startTime;
    const bytesPerSecond = elapsed > 0 ? (this.bytesReceived / elapsed) * 1000 : 0;
    
    return {
      queueLength: this.outputQueue.length,
      bytesReceived: this.bytesReceived,
      bytesPerSecond: Math.round(bytesPerSecond),
      processing: this.processing
    };
  }
}

// Export for use in terminal
if (typeof module !== 'undefined' && module.exports) {
  module.exports = OutputHandler;
}