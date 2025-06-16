const fs = require('fs');
const path = require('path');

// Read the Three.js module
const threeModule = fs.readFileSync(
  path.join(__dirname, 'node_modules/three/build/three.module.js'),
  'utf8'
);

// Create a UMD wrapper
const umdWrapper = `
(function (global, factory) {
  typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports) :
  typeof define === 'function' && define.amd ? define(['exports'], factory) :
  (global = global || self, factory(global.THREE = {}));
}(this, function (exports) {
  'use strict';
  
  ${threeModule}
  
  // Export all the things
  Object.keys(exports).forEach(function(key) {
    if (key !== 'default') {
      global[key] = exports[key];
    }
  });
  
  // Also set global THREE
  global.THREE = exports;
}));
`;

// Write the bundled file
fs.writeFileSync(
  path.join(__dirname, 'public/js/three.min.js'),
  umdWrapper
);

console.log('✓ Three.js bundle created successfully');