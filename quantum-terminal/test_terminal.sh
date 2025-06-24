#!/bin/bash

# Test script for quantum terminal functionality

echo "Testing Quantum Terminal..."
echo ""
echo "This test will:"
echo "1. Launch the terminal"
echo "2. Run some basic commands"
echo "3. Test input/output"
echo ""

# Create a test script to run inside the terminal
cat > /tmp/quantum_test_commands.sh << 'EOF'
#!/bin/bash
echo "=== Quantum Terminal Test ==="
echo "Date: $(date)"
echo "User: $(whoami)"
echo "Directory: $(pwd)"
echo ""
echo "Testing ANSI colors:"
echo -e "\033[31mRed\033[0m \033[32mGreen\033[0m \033[33mYellow\033[0m \033[34mBlue\033[0m"
echo ""
echo "Testing cursor movement..."
echo -e "Line 1\nLine 2\nLine 3"
echo -e "\033[2A\033[10CMoving cursor"
echo ""
echo "Test completed!"
EOF

chmod +x /tmp/quantum_test_commands.sh

echo "Choose terminal to test:"
echo "1) GTK Terminal (build/linux/quantum-terminal-gtk)"
echo "2) Standard Terminal (build/quantum-terminal)"
read -p "Enter choice (1 or 2): " choice

case $choice in
    1)
        echo "Launching GTK Terminal..."
        echo "Please run the test commands manually in the terminal window"
        echo "Commands to test:"
        echo "  - ls -la"
        echo "  - echo 'Hello Quantum Terminal'"
        echo "  - /tmp/quantum_test_commands.sh"
        ./build/linux/quantum-terminal-gtk
        ;;
    2)
        echo "Launching Standard Terminal..."
        ./build/quantum-terminal
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac