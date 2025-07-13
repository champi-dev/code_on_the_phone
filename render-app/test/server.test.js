const request = require('supertest');
const { describe, it, expect, beforeEach, afterEach } = require('@jest/globals');
const bcrypt = require('bcryptjs');
const fs = require('fs');
const path = require('path');
const http = require('http');

// Mock modules
jest.mock('ws');
jest.mock('child_process');
jest.mock('http-proxy-middleware');
jest.mock('dotenv');

describe('Server Tests', () => {
  let app;
  let server;
  let sessionStore;
  
  beforeEach(() => {
    // Clear module cache
    jest.resetModules();
    jest.clearAllMocks();
    
    // Set up environment
    process.env.PORT = '3002';
    process.env.PASSWORD_HASH = bcrypt.hashSync('testpass123', 10);
    process.env.SESSION_SECRET = 'test-secret';
    process.env.TERMINAL_HOST = '127.0.0.1';
    process.env.TERMINAL_PORT = '7681';
    
    // Mock file system for sessions
    jest.spyOn(fs, 'existsSync').mockReturnValue(true);
    jest.spyOn(fs, 'mkdirSync').mockImplementation(() => {});
    
    // Load app after mocks are set
    app = require('../server');
  });

  afterEach(() => {
    if (server) {
      server.close();
    }
    jest.restoreAllMocks();
  });

  describe('Authentication Tests', () => {
    it('should redirect to login when not authenticated', async () => {
      const res = await request(app).get('/');
      expect(res.status).toBe(302);
      expect(res.headers.location).toBe('/login');
    });

    it('should serve login page', async () => {
      const res = await request(app).get('/login');
      expect(res.status).toBe(200);
    });

    it('should accept valid password', async () => {
      const res = await request(app)
        .post('/api/login')
        .send({ password: 'testpass123' });
      
      expect(res.status).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.sessionInfo).toHaveProperty('expiresIn');
    });

    it('should reject invalid password', async () => {
      const res = await request(app)
        .post('/api/login')
        .send({ password: 'wrongpass' });
      
      expect(res.status).toBe(401);
      expect(res.body.success).toBe(false);
    });

    it('should handle login without password', async () => {
      const res = await request(app)
        .post('/api/login')
        .send({});
      
      expect(res.status).toBe(401);
      expect(res.body.success).toBe(false);
    });

    it('should handle bcrypt comparison error', async () => {
      jest.spyOn(bcrypt, 'compare').mockRejectedValue(new Error('bcrypt error'));
      
      const res = await request(app)
        .post('/api/login')
        .send({ password: 'testpass123' });
      
      expect(res.status).toBe(500);
      expect(res.body.success).toBe(false);
      expect(res.body.message).toBe('Server error');
    });

    it('should logout successfully', async () => {
      const agent = request.agent(app);
      
      // Login first
      await agent
        .post('/api/login')
        .send({ password: 'testpass123' });
      
      // Then logout
      const res = await agent.post('/api/logout');
      expect(res.status).toBe(200);
      expect(res.body.success).toBe(true);
    });

    it('should handle logout with process cleanup enabled', async () => {
      process.env.ENABLE_PROCESS_CLEANUP = 'true';
      const { exec } = require('child_process');
      exec.mockImplementation((cmd, cb) => cb(null, '', ''));
      
      const agent = request.agent(app);
      await agent.post('/api/login').send({ password: 'testpass123' });
      
      const res = await agent.post('/api/logout');
      expect(res.status).toBe(200);
      expect(exec).toHaveBeenCalled();
    });

    it('should handle logout with reboot enabled', async () => {
      process.env.ENABLE_REBOOT_ON_LOGOUT = 'true';
      const { exec } = require('child_process');
      exec.mockImplementation((cmd, cb) => cb(null, '', ''));
      
      const agent = request.agent(app);
      await agent.post('/api/login').send({ password: 'testpass123' });
      
      const res = await agent.post('/api/logout');
      expect(res.status).toBe(200);
      
      // Wait for reboot timeout
      await new Promise(resolve => setTimeout(resolve, 2500));
      expect(exec).toHaveBeenCalledWith(expect.stringContaining('reboot'), expect.any(Function));
    });
  });

  describe('Protected Routes', () => {
    let agent;

    beforeEach(async () => {
      agent = request.agent(app);
      await agent
        .post('/api/login')
        .send({ password: 'testpass123' });
    });

    it('should access home page when authenticated', async () => {
      const res = await agent.get('/');
      expect(res.status).toBe(200);
    });

    it('should get terminal config', async () => {
      const res = await agent.get('/api/terminal-config');
      expect(res.status).toBe(200);
      expect(res.body).toHaveProperty('host');
      expect(res.body).toHaveProperty('port');
      expect(res.body).toHaveProperty('url');
    });

    it('should get session status', async () => {
      const res = await agent.get('/api/session-status');
      expect(res.status).toBe(200);
      expect(res.body.authenticated).toBe(true);
      expect(res.body).toHaveProperty('loginTime');
      expect(res.body).toHaveProperty('sessionExpiry');
    });

    it('should execute safe commands', async () => {
      const { exec } = require('child_process');
      exec.mockImplementation((cmd, opts, cb) => cb(null, 'test output', ''));
      
      const res = await agent
        .post('/api/exec')
        .send({ command: 'echo test' });
      
      expect(res.status).toBe(200);
      expect(res.body.output).toBe('test output');
    });

    it('should reject dangerous commands', async () => {
      const res = await agent
        .post('/api/exec')
        .send({ command: 'rm -rf /' });
      
      expect(res.status).toBe(403);
      expect(res.body.error).toContain('not allowed');
    });

    it('should handle command execution errors', async () => {
      const { exec } = require('child_process');
      exec.mockImplementation((cmd, opts, cb) => 
        cb(new Error('Command failed'), '', 'error output')
      );
      
      const res = await agent
        .post('/api/exec')
        .send({ command: 'test command' });
      
      expect(res.status).toBe(200);
      expect(res.body.error).toBe('Command failed');
      expect(res.body.stderr).toBe('error output');
    });

    it('should reject exec without command', async () => {
      const res = await agent
        .post('/api/exec')
        .send({});
      
      expect(res.status).toBe(400);
      expect(res.body.error).toBe('Command required');
    });

    it('should check terminal health', async () => {
      // Mock http request
      const mockRequest = {
        on: jest.fn((event, cb) => {
          if (event === 'error') {
            cb(new Error('Connection failed'));
          }
        }),
        end: jest.fn()
      };
      
      jest.spyOn(http, 'request').mockReturnValue(mockRequest);
      
      const res = await agent.get('/api/terminal-health');
      expect(res.status).toBe(200);
      expect(res.body.available).toBe(false);
    });

    it('should test proxy configuration', async () => {
      const mockRequest = {
        on: jest.fn((event, cb) => {
          if (event === 'error') {
            cb({ message: 'Connection error', code: 'ECONNREFUSED' });
          }
        }),
        write: jest.fn(),
        end: jest.fn()
      };
      
      jest.spyOn(http, 'request').mockReturnValue(mockRequest);
      
      const res = await agent.get('/api/proxy-test');
      expect(res.status).toBe(200);
      expect(res.body.testResult.error).toBe('Connection error');
    });

    it('should get terminal debug info', async () => {
      const res = await agent.get('/api/terminal-debug');
      expect(res.status).toBe(200);
      expect(res.body).toHaveProperty('terminalUrl');
      expect(res.body).toHaveProperty('instructions');
    });
  });

  describe('Static Files and Health', () => {
    it('should serve health check', async () => {
      const res = await request(app).get('/health');
      expect(res.status).toBe(200);
      expect(res.body.status).toBe('ok');
    });

    it('should serve favicon', async () => {
      const res = await request(app).get('/favicon.ico');
      expect(res.status).toBe(200);
    });

    it('should serve service worker', async () => {
      const res = await request(app).get('/sw.js');
      expect(res.status).toBe(200);
    });

    it('should serve manifest', async () => {
      const res = await request(app).get('/manifest.json');
      expect(res.status).toBe(200);
    });
  });

  describe('WebSocket Handling', () => {
    it('should handle WebSocket upgrade for terminal proxy', (done) => {
      const server = http.createServer(app);
      server.listen(3003, () => {
        const req = {
          url: '/terminal-proxy/ws',
          headers: { cookie: 'sessionId=test123' }
        };
        const socket = { 
          write: jest.fn(),
          destroy: jest.fn()
        };
        
        server.emit('upgrade', req, socket, Buffer.alloc(0));
        
        // Expect authentication check
        setTimeout(() => {
          expect(socket.write).toHaveBeenCalled();
          server.close(done);
        }, 100);
      });
    });

    it('should reject WebSocket without authentication', (done) => {
      const server = http.createServer(app);
      server.listen(3004, () => {
        const req = {
          url: '/ws/terminal',
          headers: {}
        };
        const socket = { 
          write: jest.fn(),
          destroy: jest.fn()
        };
        
        server.emit('upgrade', req, socket, Buffer.alloc(0));
        
        setTimeout(() => {
          expect(socket.write).toHaveBeenCalledWith(
            expect.stringContaining('401 Unauthorized')
          );
          expect(socket.destroy).toHaveBeenCalled();
          server.close(done);
        }, 100);
      });
    });
  });

  describe('Terminal Proxy Fallback', () => {
    let agent;

    beforeEach(async () => {
      agent = request.agent(app);
      await agent
        .post('/api/login')
        .send({ password: 'testpass123' });
    });

    it('should serve fallback UI when requested', async () => {
      const res = await agent.get('/terminal-proxy?fallback=true');
      expect(res.status).toBe(200);
      expect(res.text).toContain('Terminal server is currently offline');
    });

    it('should handle proxy errors with fallback HTML', async () => {
      const { createProxyMiddleware } = require('http-proxy-middleware');
      const mockProxy = jest.fn((req, res, next) => {
        // Simulate proxy error
        const error = new Error('Connection refused');
        error.code = 'ECONNREFUSED';
        mockProxy.emit('error', error, req, res);
      });
      mockProxy.upgrade = jest.fn();
      createProxyMiddleware.mockReturnValue(mockProxy);
      
      const res = await agent.get('/terminal-proxy');
      expect(res.status).toBe(200);
      expect(res.text).toContain('Terminal server unavailable');
    });
  });

  describe('Session Management', () => {
    it('should create sessions directory if not exists', () => {
      fs.existsSync.mockReturnValue(false);
      jest.resetModules();
      
      require('../server');
      
      expect(fs.mkdirSync).toHaveBeenCalledWith(
        expect.stringContaining('sessions'),
        { recursive: true }
      );
    });

    it('should handle session save errors during login', async () => {
      const mockSession = {
        save: jest.fn(cb => cb(new Error('Session save failed')))
      };
      
      app.request.session = mockSession;
      
      const res = await request(app)
        .post('/api/login')
        .send({ password: 'testpass123' });
      
      expect(res.status).toBe(500);
    });

    it('should update last activity on authenticated requests', async () => {
      const agent = request.agent(app);
      await agent.post('/api/login').send({ password: 'testpass123' });
      
      const res1 = await agent.get('/api/session-status');
      const activity1 = res1.body.lastActivity;
      
      // Wait a bit
      await new Promise(resolve => setTimeout(resolve, 100));
      
      const res2 = await agent.get('/api/session-status');
      const activity2 = res2.body.lastActivity;
      
      expect(new Date(activity2).getTime()).toBeGreaterThan(new Date(activity1).getTime());
    });
  });

  describe('Rate Limiting', () => {
    it('should apply rate limiting', async () => {
      // Make many requests quickly
      const promises = [];
      for (let i = 0; i < 101; i++) {
        promises.push(request(app).get('/health'));
      }
      
      const results = await Promise.all(promises);
      const rateLimited = results.some(res => res.status === 429);
      
      // Should eventually hit rate limit
      expect(rateLimited).toBe(true);
    });
  });

  describe('Error Handling', () => {
    it('should handle missing WebSocket module', () => {
      jest.doMock('ws', () => {
        throw new Error('Module not found');
      });
      
      jest.resetModules();
      const app = require('../server');
      
      // Should not crash
      expect(app).toBeDefined();
    });

    it('should return 401 JSON for API routes when not authenticated', async () => {
      const res = await request(app).get('/api/terminal-config');
      expect(res.status).toBe(401);
      expect(res.body.error).toBe('Unauthorized');
      expect(res.body.redirect).toBe('/login');
    });
  });
});