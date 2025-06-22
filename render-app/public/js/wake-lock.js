// Wake Lock API implementation to prevent device sleep
class WakeLockManager {
  constructor() {
    this.wakeLock = null;
    this.isSupported = 'wakeLock' in navigator;
    this.isActive = false;
    this.retryCount = 0;
    this.maxRetries = 3;
    
    if (this.isSupported) {
      this.initialize();
    } else {
      console.warn('Wake Lock API not supported on this device');
    }
  }

  async initialize() {
    // Request wake lock on initialization
    await this.requestWakeLock();
    
    // Re-acquire wake lock when page becomes visible
    document.addEventListener('visibilitychange', async () => {
      if (document.visibilityState === 'visible' && !this.isActive) {
        await this.requestWakeLock();
      }
    });
    
    // Re-acquire wake lock when page is focused
    window.addEventListener('focus', async () => {
      if (!this.isActive) {
        await this.requestWakeLock();
      }
    });
    
    // Handle page show event (coming back from bfcache)
    window.addEventListener('pageshow', async (event) => {
      if (event.persisted && !this.isActive) {
        await this.requestWakeLock();
      }
    });
  }

  async requestWakeLock() {
    if (!this.isSupported) return false;
    
    try {
      // Request a screen wake lock
      this.wakeLock = await navigator.wakeLock.request('screen');
      this.isActive = true;
      this.retryCount = 0;
      
      console.log('Wake lock acquired');
      
      // Listen for release event
      this.wakeLock.addEventListener('release', () => {
        console.log('Wake lock released');
        this.isActive = false;
        
        // Try to re-acquire if page is still visible
        if (document.visibilityState === 'visible') {
          setTimeout(() => this.requestWakeLock(), 1000);
        }
      });
      
      return true;
    } catch (err) {
      console.error('Failed to acquire wake lock:', err);
      this.isActive = false;
      
      // Retry with exponential backoff
      if (this.retryCount < this.maxRetries) {
        this.retryCount++;
        const delay = Math.pow(2, this.retryCount) * 1000;
        setTimeout(() => this.requestWakeLock(), delay);
      }
      
      return false;
    }
  }

  async releaseWakeLock() {
    if (this.wakeLock) {
      try {
        await this.wakeLock.release();
        this.wakeLock = null;
        this.isActive = false;
        console.log('Wake lock released manually');
      } catch (err) {
        console.error('Failed to release wake lock:', err);
      }
    }
  }

  getStatus() {
    return {
      supported: this.isSupported,
      active: this.isActive,
      retryCount: this.retryCount
    };
  }
}

// Export for use in main application
window.WakeLockManager = WakeLockManager;