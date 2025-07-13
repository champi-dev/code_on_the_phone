const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 8080;
const HOST = '0.0.0.0';

const mimeTypes = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.wasm': 'application/wasm',
  '.json': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml'
};

const server = http.createServer((req, res) => {
  let filePath = '.' + req.url;
  if (filePath === './') {
    filePath = './index.html';
  }

  const extname = path.extname(filePath).toLowerCase();
  const contentType = mimeTypes[extname] || 'application/octet-stream';

  fs.readFile(filePath, (error, content) => {
    if (error) {
      if (error.code === 'ENOENT') {
        res.writeHead(404, { 'Content-Type': 'text/html' });
        res.end('<h1>404 - File not found</h1>', 'utf-8');
      } else {
        res.writeHead(500);
        res.end('Server error: ' + error.code, 'utf-8');
      }
    } else {
      // Add CORS headers for WASM
      res.writeHead(200, {
        'Content-Type': contentType,
        'Cross-Origin-Embedder-Policy': 'require-corp',
        'Cross-Origin-Opener-Policy': 'same-origin'
      });
      res.end(content, 'utf-8');
    }
  });
});

server.listen(PORT, HOST, () => {
  console.log(`Server running at http://${HOST}:${PORT}/`);
  console.log(`Access locally at: http://localhost:${PORT}/`);
  console.log(`Access from network at: http://100.111.108.108:${PORT}/`);
  console.log('\nPress Ctrl+C to stop the server');
});