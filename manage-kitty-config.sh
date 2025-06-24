#!/bin/bash

# Script to manage Kitty configuration with git

KITTY_DIR="$HOME/.config/kitty"

echo "Managing Kitty configuration with git..."

# Initialize git repo if not exists
if [ ! -d "$KITTY_DIR/.git" ]; then
    cd "$KITTY_DIR"
    git init
    echo "Initialized git repository in $KITTY_DIR"
fi

cd "$KITTY_DIR"

# Add all files
git add .

# Show status
echo -e "\nGit status:"
git status --short

# Commit
git commit -m "Add advanced Kitty terminal configuration

- Enhanced main configuration with layouts, keybindings, and performance settings
- Added session templates for development and system monitoring workflows
- Created multiple color themes (Dracula, Gruvbox, Tokyo Night)
- Implemented theme switcher script
- Added shell aliases for common Kitty operations

🤖 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>"

echo -e "\nConfiguration committed!"

# Ask about remote
read -p "Do you have a remote repository URL for your Kitty config? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    read -p "Enter remote URL: " REMOTE_URL
    git remote add origin "$REMOTE_URL" 2>/dev/null || git remote set-url origin "$REMOTE_URL"
    git push -u origin main
    echo "Pushed to remote!"
else
    echo "Skipping push - no remote configured"
    echo "To add a remote later, run:"
    echo "  cd $KITTY_DIR"
    echo "  git remote add origin <your-repo-url>"
    echo "  git push -u origin main"
fi