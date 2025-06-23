#!/bin/bash
set -e

echo "🚀 Quantum Terminal - App Store Deployment Script"
echo "================================================"

# Check if Xcode is installed
if ! command -v xcodebuild &> /dev/null; then
    echo "❌ Error: Xcode is not installed. Please install Xcode from the Mac App Store."
    exit 1
fi

# Configuration
PROJECT_NAME="QuantumTerminal"
SCHEME_NAME="QuantumTerminal"
BUNDLE_ID="com.quantum.terminal"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Setup Xcode project
echo -e "\n${YELLOW}Step 1: Setting up Xcode project${NC}"

if [ ! -d "$PROJECT_NAME.xcodeproj" ]; then
    echo "Creating Xcode project..."
    # Project already created above
else
    echo "✅ Xcode project exists"
fi

# Step 2: Check code signing
echo -e "\n${YELLOW}Step 2: Checking code signing${NC}"
TEAM_ID=$(security find-identity -v -p codesigning | grep "Developer ID" | head -1 | awk -F'"' '{print $2}' | awk '{print $NF}' | tr -d '()')

if [ -z "$TEAM_ID" ]; then
    echo -e "${RED}❌ No valid code signing identity found${NC}"
    echo "Please:"
    echo "1. Open Xcode"
    echo "2. Go to Preferences → Accounts"
    echo "3. Add your Apple ID"
    echo "4. Download signing certificates"
    exit 1
else
    echo "✅ Found Team ID: $TEAM_ID"
fi

# Step 3: Update project settings
echo -e "\n${YELLOW}Step 3: Updating project settings${NC}"
/usr/libexec/PlistBuddy -c "Set :DEVELOPMENT_TEAM $TEAM_ID" "$PROJECT_NAME.xcodeproj/project.pbxproj"
echo "✅ Updated Team ID in project"

# Step 4: Clean build folder
echo -e "\n${YELLOW}Step 4: Cleaning build folder${NC}"
rm -rf build/
xcodebuild clean -project "$PROJECT_NAME.xcodeproj" -scheme "$SCHEME_NAME" 2>/dev/null || true
echo "✅ Cleaned build folder"

# Step 5: Build for macOS
echo -e "\n${YELLOW}Step 5: Building for macOS${NC}"
xcodebuild archive \
    -project "$PROJECT_NAME.xcodeproj" \
    -scheme "$SCHEME_NAME" \
    -configuration Release \
    -archivePath "build/$PROJECT_NAME-macOS.xcarchive" \
    -destination "platform=macOS" \
    PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID" \
    CODE_SIGN_STYLE="Automatic" \
    DEVELOPMENT_TEAM="$TEAM_ID"

if [ $? -eq 0 ]; then
    echo "✅ macOS build successful"
else
    echo -e "${RED}❌ macOS build failed${NC}"
    exit 1
fi

# Step 6: Build for iOS
echo -e "\n${YELLOW}Step 6: Building for iOS${NC}"
xcodebuild archive \
    -project "$PROJECT_NAME.xcodeproj" \
    -scheme "$SCHEME_NAME" \
    -configuration Release \
    -archivePath "build/$PROJECT_NAME-iOS.xcarchive" \
    -destination "generic/platform=iOS" \
    PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID" \
    CODE_SIGN_STYLE="Automatic" \
    DEVELOPMENT_TEAM="$TEAM_ID"

if [ $? -eq 0 ]; then
    echo "✅ iOS build successful"
else
    echo -e "${RED}❌ iOS build failed${NC}"
    exit 1
fi

# Step 7: Export for App Store
echo -e "\n${YELLOW}Step 7: Exporting for App Store${NC}"

# Update ExportOptions with actual Team ID
sed -i '' "s/YOUR_TEAM_ID/$TEAM_ID/g" ExportOptions-AppStore.plist

xcodebuild -exportArchive \
    -archivePath "build/$PROJECT_NAME-macOS.xcarchive" \
    -exportPath "build/export-macos" \
    -exportOptionsPlist ExportOptions-AppStore.plist

xcodebuild -exportArchive \
    -archivePath "build/$PROJECT_NAME-iOS.xcarchive" \
    -exportPath "build/export-ios" \
    -exportOptionsPlist ExportOptions-AppStore.plist

echo "✅ Export complete"

# Step 8: Validate before upload
echo -e "\n${YELLOW}Step 8: Validating builds${NC}"

# Check if user has API key
if [ -f "~/.appstoreconnect/private_keys/AuthKey_*.p8" ]; then
    echo "Using API key for validation..."
    API_KEY=$(ls ~/.appstoreconnect/private_keys/AuthKey_*.p8 | head -1 | xargs basename | sed 's/AuthKey_//' | sed 's/.p8//')
    xcrun altool --validate-app \
        -f "build/export-macos/$PROJECT_NAME.pkg" \
        -t macos \
        --apiKey "$API_KEY" \
        --apiIssuer "YOUR_ISSUER_ID"
else
    echo "Using Apple ID for validation..."
    echo "Enter your Apple ID:"
    read APPLE_ID
    xcrun altool --validate-app \
        -f "build/export-ios/$PROJECT_NAME.ipa" \
        -t ios \
        -u "$APPLE_ID" \
        -p "@keychain:AC_PASSWORD"
fi

echo -e "\n${GREEN}✅ Build complete!${NC}"
echo -e "\nNext steps:"
echo "1. Open Xcode → Window → Organizer"
echo "2. Select your archive"
echo "3. Click 'Distribute App'"
echo "4. Choose 'App Store Connect'"
echo "5. Follow the upload wizard"
echo -e "\nAlternatively, upload manually:"
echo "  macOS: build/export-macos/$PROJECT_NAME.pkg"
echo "  iOS: build/export-ios/$PROJECT_NAME.ipa"

# Create README for team
cat > build/UPLOAD_INSTRUCTIONS.md << EOF
# Upload Instructions

## Automatic Upload (Recommended)
1. Open Xcode
2. Window → Organizer
3. Select the archive
4. Click "Distribute App"
5. Choose "App Store Connect"

## Manual Upload
1. Go to https://appstoreconnect.apple.com
2. My Apps → Quantum Terminal
3. Click (+) Version
4. Upload builds:
   - macOS: build/export-macos/$PROJECT_NAME.pkg
   - iOS: build/export-ios/$PROJECT_NAME.ipa

## Using Transporter
1. Download Transporter from Mac App Store
2. Sign in with Apple ID
3. Drag the .ipa/.pkg files
4. Click Deliver

## TestFlight
After upload:
1. Wait for processing (10-30 minutes)
2. Go to TestFlight tab
3. Add internal testers
4. Submit for Beta App Review
5. Once approved, add external testers
EOF

echo -e "\n📄 Created build/UPLOAD_INSTRUCTIONS.md"

# Open Xcode Organizer
echo -e "\n${YELLOW}Opening Xcode Organizer...${NC}"
open -a Xcode
osascript -e 'tell application "Xcode" to activate' -e 'tell application "System Events" to keystroke "O" using {command down, shift down}'

echo -e "\n${GREEN}🎉 Deployment preparation complete!${NC}"