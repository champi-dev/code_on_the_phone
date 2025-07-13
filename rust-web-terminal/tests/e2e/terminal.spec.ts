import { test, expect } from '@playwright/test';

test.describe('Rust Web Terminal', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should load the terminal interface', async ({ page }) => {
    // Take screenshot of initial load
    await page.screenshot({ path: 'screenshots/01-initial-load.png', fullPage: true });
    
    // Check header
    await expect(page.locator('h1')).toContainText('Rust Web Terminal');
    
    // Check connection status
    await expect(page.locator('#status')).toHaveClass(/disconnected/);
    await expect(page.locator('#status')).toContainText('Disconnected');
    
    // Check connect dialog is visible
    await expect(page.locator('#connect-dialog')).toBeVisible();
    
    // Check terminal container exists
    await expect(page.locator('#terminal-container')).toBeVisible();
  });

  test('should show connection dialog with pre-filled values', async ({ page }) => {
    // Take screenshot of connection dialog
    await page.screenshot({ path: 'screenshots/02-connection-dialog.png', fullPage: true });
    
    // Check host input
    const hostInput = page.locator('#host-input');
    await expect(hostInput).toHaveValue('142.93.249.123');
    
    // Check password input
    const passwordInput = page.locator('#password-input');
    await expect(passwordInput).toHaveValue('cloudterm123');
    
    // Check connect button
    const connectButton = page.locator('#connect-button');
    await expect(connectButton).toBeEnabled();
    await expect(connectButton).toContainText('Connect');
  });

  test('should validate empty inputs', async ({ page }) => {
    // Clear inputs
    await page.fill('#host-input', '');
    await page.fill('#password-input', '');
    
    // Try to connect
    await page.click('#connect-button');
    
    // Check error message
    await expect(page.locator('#error-message')).toBeVisible();
    await expect(page.locator('#error-message')).toContainText('Please enter host and password');
    
    // Take screenshot of validation error
    await page.screenshot({ path: 'screenshots/03-validation-error.png', fullPage: true });
  });

  test('should attempt connection to droplet', async ({ page }) => {
    // Click connect button
    await page.click('#connect-button');
    
    // Check connecting state
    await expect(page.locator('#status')).toHaveClass(/connecting/);
    await expect(page.locator('#connect-button')).toBeDisabled();
    await expect(page.locator('#connect-button')).toContainText('Connecting...');
    
    // Take screenshot of connecting state
    await page.screenshot({ path: 'screenshots/04-connecting.png', fullPage: true });
    
    // Wait for connection result (success or failure)
    await page.waitForTimeout(5000);
    
    // Take screenshot of final state
    await page.screenshot({ path: 'screenshots/05-connection-result.png', fullPage: true });
    
    // Check if connection succeeded
    const status = await page.locator('#status').getAttribute('class');
    if (status?.includes('connected')) {
      // Connection successful
      await expect(page.locator('#connect-dialog')).not.toBeVisible();
      await expect(page.locator('#status')).toContainText('Connected');
      
      // Check terminal has content
      await page.waitForTimeout(2000);
      const terminalContent = await page.locator('.xterm-screen').textContent();
      expect(terminalContent).toBeTruthy();
      
      // Take screenshot of connected terminal
      await page.screenshot({ path: 'screenshots/06-connected-terminal.png', fullPage: true });
    } else {
      // Connection failed
      await expect(page.locator('#error-message')).toBeVisible();
      await expect(page.locator('#connect-button')).toBeEnabled();
    }
  });

  test('should handle terminal interaction', async ({ page }) => {
    // Connect first
    await page.click('#connect-button');
    
    // Wait for connection
    await page.waitForTimeout(5000);
    
    // If connected, try typing
    const status = await page.locator('#status').getAttribute('class');
    if (status?.includes('connected')) {
      // Focus terminal
      await page.click('#terminal');
      
      // Type a command
      await page.keyboard.type('ls -la');
      await page.screenshot({ path: 'screenshots/07-typing-command.png', fullPage: true });
      
      // Press Enter
      await page.keyboard.press('Enter');
      await page.waitForTimeout(2000);
      
      // Take screenshot of command output
      await page.screenshot({ path: 'screenshots/08-command-output.png', fullPage: true });
      
      // Type another command
      await page.keyboard.type('pwd');
      await page.keyboard.press('Enter');
      await page.waitForTimeout(1000);
      
      // Take final screenshot
      await page.screenshot({ path: 'screenshots/09-multiple-commands.png', fullPage: true });
    }
  });

  test('should handle window resize', async ({ page }) => {
    // Set initial viewport
    await page.setViewportSize({ width: 1200, height: 800 });
    await page.screenshot({ path: 'screenshots/10-normal-size.png', fullPage: true });
    
    // Connect
    await page.click('#connect-button');
    await page.waitForTimeout(5000);
    
    // Resize to mobile
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(1000);
    await page.screenshot({ path: 'screenshots/11-mobile-size.png', fullPage: true });
    
    // Resize to tablet
    await page.setViewportSize({ width: 768, height: 1024 });
    await page.waitForTimeout(1000);
    await page.screenshot({ path: 'screenshots/12-tablet-size.png', fullPage: true });
  });
});

test.describe('Terminal Performance', () => {
  test('should handle rapid input', async ({ page }) => {
    await page.goto('/');
    await page.click('#connect-button');
    await page.waitForTimeout(5000);
    
    const status = await page.locator('#status').getAttribute('class');
    if (status?.includes('connected')) {
      // Focus terminal
      await page.click('#terminal');
      
      // Type rapidly
      const startTime = Date.now();
      for (let i = 0; i < 100; i++) {
        await page.keyboard.type(`echo "Line ${i}"`, { delay: 0 });
        await page.keyboard.press('Enter', { delay: 0 });
      }
      const endTime = Date.now();
      
      console.log(`Typed 100 commands in ${endTime - startTime}ms`);
      
      await page.waitForTimeout(3000);
      await page.screenshot({ path: 'screenshots/13-rapid-input.png', fullPage: true });
    }
  });

  test('should measure connection time', async ({ page }) => {
    await page.goto('/');
    
    const startTime = Date.now();
    await page.click('#connect-button');
    
    // Wait for connection result
    await page.waitForFunction(
      () => {
        const status = document.querySelector('#status');
        return status?.className.includes('connected') || 
               status?.className.includes('disconnected');
      },
      { timeout: 30000 }
    );
    
    const endTime = Date.now();
    const connectionTime = endTime - startTime;
    
    console.log(`Connection attempt took ${connectionTime}ms`);
    
    // Assert reasonable connection time
    expect(connectionTime).toBeLessThan(10000); // Should connect within 10 seconds
  });
});