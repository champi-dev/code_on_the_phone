const bcrypt = require('bcryptjs');

// The hash from Render environment
const existingHash = '$2a$10$xK1.BKDWYUQvtVl.W3Mjz.8rZKgX6IH5EYXL3jN8ifYJnL3GpXWlm';

// Common passwords to test
const passwords = [
  'cloudterm123',
  'admin',
  'password',
  'terminal',
  'cloud',
  '123456',
  'admin123',
  'terminal123',
  'render',
  'CloudTerm123',
  'Cloudterm123',
  'CLOUDTERM123'
];

console.log('Testing passwords against hash:', existingHash);
console.log('================================================\n');

let found = false;
for (const password of passwords) {
  const matches = bcrypt.compareSync(password, existingHash);
  if (matches) {
    console.log(`✓ FOUND! Password is: "${password}"`);
    found = true;
    break;
  } else {
    console.log(`✗ Not: ${password}`);
  }
}

if (!found) {
  console.log('\n❌ None of the common passwords matched.');
  console.log('The password might be something custom.');
  console.log('\nTo fix this, update the PASSWORD_HASH in Render with a new hash.');
  console.log('Run: node generate-password-hash.js');
}