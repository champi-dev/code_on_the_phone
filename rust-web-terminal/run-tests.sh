#!/bin/bash

set -e

echo "=== Rust Web Terminal Test Suite ==="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Create directories
mkdir -p screenshots test-results

# Step 1: Build the project
echo -e "${YELLOW}Building WASM module...${NC}"
./build.sh

# Step 2: Run Rust unit tests
echo -e "\n${YELLOW}Running Rust unit tests...${NC}"
wasm-pack test --chrome --headless

# Step 3: Install Node dependencies if needed
if [ ! -d "node_modules" ]; then
    echo -e "\n${YELLOW}Installing test dependencies...${NC}"
    npm install
fi

# Step 4: Install Playwright browsers if needed
if [ ! -d "$HOME/.cache/ms-playwright" ]; then
    echo -e "\n${YELLOW}Installing Playwright browsers...${NC}"
    npx playwright install
fi

# Step 5: Run E2E tests
echo -e "\n${YELLOW}Running E2E tests with screenshots...${NC}"
npm test

# Step 6: Generate test report
echo -e "\n${YELLOW}Generating test report...${NC}"
npx playwright show-report --host 0.0.0.0 --port 9323 &
REPORT_PID=$!

echo -e "\n${GREEN}=== Test Results ===${NC}"
echo

# Show test summary
if [ -f "test-results/results.json" ]; then
    echo "Test results saved to test-results/results.json"
fi

# List screenshots
echo -e "\n${GREEN}Screenshots captured:${NC}"
ls -la screenshots/*.png 2>/dev/null || echo "No screenshots found"

# Show coverage if available
if [ -f "coverage/index.html" ]; then
    echo -e "\n${GREEN}Code coverage report available at coverage/index.html${NC}"
fi

echo -e "\n${GREEN}Test report server running at http://localhost:9323${NC}"
echo "Press Ctrl+C to stop the report server"

# Wait for report server
wait $REPORT_PID