module.exports = {
  testEnvironment: 'node',
  testMatch: ['**/test/**/*.test.js'],
  collectCoverageFrom: [
    'server.js',
    'public/js/connection-worker.js',
    'public/js/output-handler.js',
    'public/js/terminal-persistence.js',
    '!**/node_modules/**',
    '!**/test/**',
    '!jest.config.js',
    '!generate-*.js',
    '!test-*.js',
    '!public/js/three.min.js'
  ],
  coverageThreshold: {
    global: {
      branches: 100,
      functions: 100,
      lines: 100,
      statements: 100
    }
  },
  coverageReporters: ['text', 'lcov', 'html'],
  coverageDirectory: 'coverage',
  testTimeout: 10000,
  verbose: true,
  setupFilesAfterEnv: ['<rootDir>/test/setup.js'],
  moduleNameMapper: {
    '^@/(.*)$': '<rootDir>/$1'
  },
  transform: {}
};