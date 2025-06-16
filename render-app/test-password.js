const bcrypt = require('bcryptjs');

// Test password
const password = 'cloudterm123';
const hash = bcrypt.hashSync(password, 10);

console.log('Password:', password);
console.log('Hash:', hash);
console.log('');

// Test verification
console.log('Testing password "cloudterm123":', bcrypt.compareSync('cloudterm123', hash));
console.log('Testing password "wrongpass":', bcrypt.compareSync('wrongpass', hash));
console.log('');

// Generate hash for environment variable
console.log('To set in Render environment:');
console.log('PASSWORD_HASH=' + hash);