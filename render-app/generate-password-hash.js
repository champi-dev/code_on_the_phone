const bcrypt = require('bcryptjs');
const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

console.log('Password Hash Generator for Cloud Terminal 3D');
console.log('============================================\n');

rl.question('Enter the password you want to use: ', (password) => {
  const hash = bcrypt.hashSync(password, 10);
  
  console.log('\n✓ Password hash generated!\n');
  console.log('Copy this hash and set it in Render environment variables:');
  console.log('------------------------------------------------------------');
  console.log(`PASSWORD_HASH=${hash}`);
  console.log('------------------------------------------------------------\n');
  console.log('Steps to update in Render:');
  console.log('1. Go to your Render dashboard');
  console.log('2. Select your Cloud Terminal 3D service');
  console.log('3. Go to Environment tab');
  console.log('4. Update PASSWORD_HASH with the value above');
  console.log('5. Save changes (service will auto-restart)\n');
  
  rl.close();
});