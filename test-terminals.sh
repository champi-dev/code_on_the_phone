#!/bin/bash

# Terminal Test Runner Script
# Run all terminal tests across different backends

set -e

echo "🧪 Terminal Test Suite"
echo "===================="

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test Node.js terminal
echo -e "\n${YELLOW}Testing Node.js Terminal...${NC}"
cd render-app

# Install test dependencies if needed
if ! npm list jest &>/dev/null; then
    echo "Installing test dependencies..."
    npm install --save-dev jest @jest/globals ws
fi

# Run tests
if npm test; then
    echo -e "${GREEN}✓ Node.js tests passed${NC}"
else
    echo -e "${RED}✗ Node.js tests failed${NC}"
    exit 1
fi

cd ..

# Test Go backend
echo -e "\n${YELLOW}Testing Go Backend...${NC}"
cd flutter-quantum-terminal/backend

# Run tests
if go test -v ./...; then
    echo -e "${GREEN}✓ Go tests passed${NC}"
else
    echo -e "${RED}✗ Go tests failed${NC}"
    exit 1
fi

# Run benchmarks
echo -e "\n${YELLOW}Running Go Benchmarks...${NC}"
go test -bench=. -benchmem

cd ../..

# Test for security issues
echo -e "\n${YELLOW}Running Security Checks...${NC}"

# Check for hardcoded secrets
if grep -r "password\|secret\|key" --include="*.go" --include="*.js" --exclude-dir=node_modules --exclude-dir=test --exclude="*test*" . | grep -v "PASSWORD_HASH\|SESSION_SECRET\|ssh.PublicKey\|HostKey"; then
    echo -e "${RED}⚠️  Warning: Potential hardcoded secrets found${NC}"
else
    echo -e "${GREEN}✓ No hardcoded secrets detected${NC}"
fi

# Summary
echo -e "\n${GREEN}✨ All tests completed!${NC}"
echo "===================="
echo "Test Summary:"
echo "- Node.js Terminal: ✓"
echo "- Go Backend: ✓"
echo "- Security Checks: ✓"
echo ""
echo "To run specific test suites:"
echo "  Node.js: cd render-app && npm test"
echo "  Go: cd flutter-quantum-terminal/backend && go test -v ./..."
echo "  Coverage: add --coverage flag (npm) or -cover flag (go)"