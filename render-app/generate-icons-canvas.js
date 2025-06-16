const { createCanvas } = require('canvas');
const fs = require('fs');
const path = require('path');

// Ensure public directory exists
const publicDir = path.join(__dirname, 'public');
if (!fs.existsSync(publicDir)) {
  fs.mkdirSync(publicDir, { recursive: true });
}

function generateIcon(size) {
  const canvas = createCanvas(size, size);
  const ctx = canvas.getContext('2d');
  
  // Create gradient background
  const gradient = ctx.createLinearGradient(0, 0, size, size);
  gradient.addColorStop(0, '#7aa2f7');
  gradient.addColorStop(1, '#bb9af7');
  
  // Draw rounded rectangle
  const radius = size * 0.2;
  ctx.fillStyle = gradient;
  ctx.beginPath();
  ctx.moveTo(radius, 0);
  ctx.lineTo(size - radius, 0);
  ctx.quadraticCurveTo(size, 0, size, radius);
  ctx.lineTo(size, size - radius);
  ctx.quadraticCurveTo(size, size, size - radius, size);
  ctx.lineTo(radius, size);
  ctx.quadraticCurveTo(0, size, 0, size - radius);
  ctx.lineTo(0, radius);
  ctx.quadraticCurveTo(0, 0, radius, 0);
  ctx.closePath();
  ctx.fill();
  
  // Draw terminal prompt
  ctx.strokeStyle = 'white';
  ctx.lineWidth = size * 0.04;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  
  // Draw ">" symbol
  const promptSize = size * 0.3;
  const centerY = size * 0.55;
  const startX = size * 0.23;
  
  ctx.beginPath();
  ctx.moveTo(startX, centerY - promptSize/2);
  ctx.lineTo(startX + promptSize/2, centerY);
  ctx.lineTo(startX, centerY + promptSize/2);
  ctx.stroke();
  
  // Draw cursor line
  ctx.beginPath();
  ctx.moveTo(size * 0.47, centerY + promptSize/2);
  ctx.lineTo(size * 0.74, centerY + promptSize/2);
  ctx.stroke();
  
  return canvas;
}

// Generate icons
try {
  // Generate 192x192 icon
  const icon192 = generateIcon(192);
  const buffer192 = icon192.toBuffer('image/png');
  fs.writeFileSync(path.join(publicDir, 'icon-192.png'), buffer192);
  console.log('✓ Generated icon-192.png');
  
  // Generate 512x512 icon
  const icon512 = generateIcon(512);
  const buffer512 = icon512.toBuffer('image/png');
  fs.writeFileSync(path.join(publicDir, 'icon-512.png'), buffer512);
  console.log('✓ Generated icon-512.png');
  
} catch (error) {
  console.error('Error:', error);
  console.log('Install canvas with: npm install canvas');
}