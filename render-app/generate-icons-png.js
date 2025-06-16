const sharp = require('sharp');
const fs = require('fs');
const path = require('path');

// Create a simple terminal icon as SVG
const iconSVG = `
<svg width="512" height="512" viewBox="0 0 512 512" xmlns="http://www.w3.org/2000/svg">
  <rect width="512" height="512" rx="100" fill="url(#grad)"/>
  <path d="M120 200 L200 280 L120 360" stroke="white" stroke-width="20" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="240" y1="360" x2="380" y2="360" stroke="white" stroke-width="20" stroke-linecap="round"/>
  <defs>
    <linearGradient id="grad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#7aa2f7;stop-opacity:1" />
      <stop offset="100%" style="stop-color:#bb9af7;stop-opacity:1" />
    </linearGradient>
  </defs>
</svg>`;

// Ensure public directory exists
const publicDir = path.join(__dirname, 'public');
if (!fs.existsSync(publicDir)) {
  fs.mkdirSync(publicDir, { recursive: true });
}

// Generate PNG icons from SVG
async function generateIcons() {
  try {
    // Generate 192x192 icon
    await sharp(Buffer.from(iconSVG))
      .resize(192, 192)
      .png()
      .toFile(path.join(publicDir, 'icon-192.png'));
    
    console.log('✓ Generated icon-192.png');
    
    // Generate 512x512 icon
    await sharp(Buffer.from(iconSVG))
      .resize(512, 512)
      .png()
      .toFile(path.join(publicDir, 'icon-512.png'));
    
    console.log('✓ Generated icon-512.png');
    
  } catch (error) {
    console.error('Error generating icons:', error);
    console.log('\nAlternative: Create icons manually or use canvas-based approach');
  }
}

generateIcons();