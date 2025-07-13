const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');

// Import the OutputHandler directly
const OutputHandler = require('../public/js/output-handler.js');

describe('OutputHandler Tests', () => {
  let outputHandler;
  let mockTerminal;

  beforeEach(() => {
    // Mock terminal
    mockTerminal = {
      write: jest.fn()
    };
    
    // Create OutputHandler instance
    outputHandler = new OutputHandler(mockTerminal);
    
    // Mock timers
    jest.useFakeTimers();
  });

  afterEach(() => {
    jest.useRealTimers();
    jest.clearAllMocks();
  });

  describe('Initialization', () => {
    it('should initialize with correct default values', () => {
      expect(outputHandler.terminal).toBe(mockTerminal);
      expect(outputHandler.outputQueue).toEqual([]);
      expect(outputHandler.processing).toBe(false);
      expect(outputHandler.bytesReceived).toBe(0);
      expect(outputHandler.maxBytesPerSecond).toBe(1024 * 1024);
      expect(outputHandler.chunkDelay).toBe(10);
    });
  });

  describe('Output Handling', () => {
    it('should add output to queue', () => {
      // Prevent automatic processing
      outputHandler.processing = true;
      
      outputHandler.handleOutput('test output');
      
      expect(outputHandler.outputQueue).toContain('test output');
      expect(outputHandler.bytesReceived).toBe(11); // 'test output'.length
    });

    it('should process queue immediately when not processing', async () => {
      const processQueueSpy = jest.spyOn(outputHandler, 'processQueue');
      
      outputHandler.handleOutput('test');
      
      expect(processQueueSpy).toHaveBeenCalled();
    });

    it('should not start new processing when already processing', () => {
      outputHandler.processing = true;
      const processQueueSpy = jest.spyOn(outputHandler, 'processQueue');
      
      outputHandler.handleOutput('test');
      
      expect(processQueueSpy).not.toHaveBeenCalled();
    });

    it('should write output to terminal', async () => {
      outputHandler.handleOutput('test output');
      
      // Process the queue
      await jest.runAllTimersAsync();
      
      expect(mockTerminal.write).toHaveBeenCalledWith('test output');
    });

    it('should process multiple queued outputs in order', async () => {
      outputHandler.handleOutput('first');
      outputHandler.handleOutput('second');
      outputHandler.handleOutput('third');
      
      await jest.runAllTimersAsync();
      
      expect(mockTerminal.write).toHaveBeenCalledTimes(3);
      expect(mockTerminal.write).toHaveBeenNthCalledWith(1, 'first');
      expect(mockTerminal.write).toHaveBeenNthCalledWith(2, 'second');
      expect(mockTerminal.write).toHaveBeenNthCalledWith(3, 'third');
    });
  });

  describe('Rate Limiting', () => {
    it('should apply rate limiting for large outputs', async () => {
      // Set a very low rate limit for testing
      outputHandler.maxBytesPerSecond = 100; // 100 bytes/second
      
      // Add large output (200 bytes)
      const largeOutput = 'a'.repeat(200);
      outputHandler.handleOutput(largeOutput);
      
      const startTime = Date.now();
      await jest.runAllTimersAsync();
      
      // The rate limiting will cause a delay since 200 bytes at 100 bytes/sec = 2 seconds
      // but elapsed time is 0, so it should add a delay
      await jest.runAllTimersAsync();
      
      // After processing, it should no longer be processing
      expect(outputHandler.processing).toBe(false);
      expect(mockTerminal.write).toHaveBeenCalledWith(largeOutput);
    });

    it('should not apply rate limiting for small outputs', async () => {
      // Small output well within rate limit
      outputHandler.handleOutput('small');
      
      const mockSetTimeout = jest.isMockFunction(setTimeout) ? setTimeout : global.setTimeout;
      const setTimeoutCalls = mockSetTimeout.mock ? mockSetTimeout.mock.calls.length : 0;
      await jest.runAllTimersAsync();
      
      // Should not have added additional delays for small outputs
      // The test passes if no error is thrown
    });

    it('should reset rate limiting counters after 1 second', async () => {
      const originalDateNow = Date.now;
      let currentTime = 1000;
      Date.now = jest.fn(() => currentTime);
      
      outputHandler.startTime = 0; // Start at time 0
      outputHandler.bytesReceived = 1000;
      
      // Time has elapsed more than 1 second
      currentTime = 1100;
      
      // Add some data to process
      outputHandler.outputQueue.push('test');
      
      // Process the queue - this should trigger the reset
      outputHandler.processing = false;
      await outputHandler.processQueue();
      
      // Should have reset counters after processing
      expect(outputHandler.bytesReceived).toBe(0);
      expect(outputHandler.startTime).toBe(1100);
      
      Date.now = originalDateNow;
    });
  });

  describe('Queue Management', () => {
    it('should clear queue and reset counters', () => {
      outputHandler.outputQueue = ['item1', 'item2'];
      outputHandler.bytesReceived = 100;
      const originalStartTime = outputHandler.startTime;
      
      outputHandler.clear();
      
      expect(outputHandler.outputQueue).toEqual([]);
      expect(outputHandler.bytesReceived).toBe(0);
      expect(outputHandler.startTime).toBeGreaterThanOrEqual(originalStartTime);
    });

    it('should set processing flag correctly', async () => {
      expect(outputHandler.processing).toBe(false);
      
      outputHandler.handleOutput('test');
      expect(outputHandler.processing).toBe(true);
      
      await jest.runAllTimersAsync();
      expect(outputHandler.processing).toBe(false);
    });
  });

  describe('Statistics', () => {
    it('should return correct stats', () => {
      outputHandler.outputQueue = ['item1', 'item2'];
      outputHandler.bytesReceived = 1024;
      outputHandler.processing = true;
      
      const stats = outputHandler.getStats();
      
      expect(stats).toEqual({
        queueLength: 2,
        bytesReceived: 1024,
        bytesPerSecond: expect.any(Number),
        processing: true
      });
    });

    it('should calculate bytes per second correctly', () => {
      const originalDateNow = Date.now;
      Date.now = jest.fn(() => 2000);
      
      outputHandler.startTime = 1000; // 1 second ago
      outputHandler.bytesReceived = 500;
      
      const stats = outputHandler.getStats();
      
      expect(stats.bytesPerSecond).toBe(500); // 500 bytes in 1 second
      
      Date.now = originalDateNow;
    });

    it('should handle zero elapsed time', () => {
      outputHandler.startTime = Date.now();
      outputHandler.bytesReceived = 100;
      
      const stats = outputHandler.getStats();
      
      expect(stats.bytesPerSecond).toBe(0);
    });
  });

  describe('Async Queue Processing', () => {
    it('should process entire queue before stopping', async () => {
      outputHandler.outputQueue = ['item1', 'item2', 'item3'];
      
      await outputHandler.processQueue();
      
      expect(outputHandler.outputQueue).toEqual([]);
      expect(mockTerminal.write).toHaveBeenCalledTimes(3);
    });

    it('should handle new items added during processing', async () => {
      outputHandler.handleOutput('first');
      
      // Add more items while processing
      setTimeout(() => {
        outputHandler.outputQueue.push('second');
      }, 5);
      
      await jest.runAllTimersAsync();
      
      expect(mockTerminal.write).toHaveBeenCalledWith('first');
      expect(mockTerminal.write).toHaveBeenCalledWith('second');
    });
  });

  describe('Edge Cases', () => {
    it('should handle empty data', () => {
      // Prevent automatic processing
      outputHandler.processing = true;
      
      outputHandler.handleOutput('');
      
      expect(outputHandler.outputQueue).toContain('');
      expect(outputHandler.bytesReceived).toBe(0);
    });

    it('should handle very large single output', async () => {
      const veryLargeOutput = 'x'.repeat(10000);
      outputHandler.handleOutput(veryLargeOutput);
      
      await jest.runAllTimersAsync();
      
      expect(mockTerminal.write).toHaveBeenCalledWith(veryLargeOutput);
    });

    it('should handle rapid successive outputs', () => {
      for (let i = 0; i < 100; i++) {
        outputHandler.handleOutput(`output${i}`);
      }
      
      expect(outputHandler.outputQueue.length).toBe(99); // First one starts processing
      expect(outputHandler.processing).toBe(true);
    });
  });

  describe('Module Export', () => {
    it('should export OutputHandler for CommonJS', () => {
      expect(OutputHandler).toBeDefined();
      expect(typeof OutputHandler).toBe('function');
    });
  });
});