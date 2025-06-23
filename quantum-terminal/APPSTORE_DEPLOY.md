# App Store Deployment Guide for Quantum Terminal

## Prerequisites

1. **Apple Developer Account** ($99/year)
   - Sign up at https://developer.apple.com
   - Enroll in Apple Developer Program

2. **Xcode 14+** installed
3. **Valid Apple ID** configured in Xcode

## Step 1: Create Xcode Project

### Option A: Use the provided Xcode project

```bash
cd quantum-terminal
open QuantumTerminal.xcodeproj
```

### Option B: Create new project

1. Open Xcode → File → New → Project
2. Choose "App" template
3. Configure:
   - Product Name: Quantum Terminal
   - Team: Your Developer Team
   - Organization Identifier: com.yourcompany
   - Bundle Identifier: com.yourcompany.quantum-terminal
   - Interface: XIB (for custom OpenGL view)
   - Language: Objective-C

## Step 2: Configure Project Settings

### General Settings
1. Set Deployment Target:
   - macOS: 10.14 minimum
   - iOS: 13.0 minimum

2. Supported Destinations:
   - Mac (Designed for iPad)
   - Mac Catalyst
   - iPhone
   - iPad

### Signing & Capabilities
1. Select your Team
2. Enable Automatic Signing
3. Add Capabilities:
   - Hardened Runtime (macOS)
   - App Sandbox (both)

### Info.plist Configuration
Add these keys:
```xml
<key>NSHighResolutionCapable</key>
<true/>
<key>LSMinimumSystemVersion</key>
<string>10.14</string>
<key>UIRequiredDeviceCapabilities</key>
<array>
    <string>armv7</string>
    <string>opengles-2</string>
</array>
<key>UISupportedInterfaceOrientations</key>
<array>
    <string>UIInterfaceOrientationPortrait</string>
    <string>UIInterfaceOrientationLandscapeLeft</string>
    <string>UIInterfaceOrientationLandscapeRight</string>
</array>
```

## Step 3: Add Your Code

1. Copy `src/cocoa_terminal.m` to the Xcode project
2. Add frameworks:
   - OpenGL.framework
   - Cocoa.framework
   - Foundation.framework
   - CoreGraphics.framework

## Step 4: Create Universal Build

### Create fat binary that works on both Mac and iOS:

1. In Xcode, create new scheme for each platform
2. Build Settings:
   - Set "Build Active Architecture Only" = NO
   - Architectures: Standard Architectures

### Build script for universal app:
```bash
#!/bin/bash
# build_universal.sh

# Clean
xcodebuild clean -project QuantumTerminal.xcodeproj -scheme QuantumTerminal

# Build for macOS
xcodebuild archive \
    -project QuantumTerminal.xcodeproj \
    -scheme "QuantumTerminal-macOS" \
    -archivePath build/QuantumTerminal-macOS.xcarchive \
    -destination "platform=macOS"

# Build for iOS
xcodebuild archive \
    -project QuantumTerminal.xcodeproj \
    -scheme "QuantumTerminal-iOS" \
    -archivePath build/QuantumTerminal-iOS.xcarchive \
    -destination "generic/platform=iOS"

# Export for App Store
xcodebuild -exportArchive \
    -archivePath build/QuantumTerminal-macOS.xcarchive \
    -exportPath build/macos \
    -exportOptionsPlist ExportOptions-AppStore.plist

xcodebuild -exportArchive \
    -archivePath build/QuantumTerminal-iOS.xcarchive \
    -exportPath build/ios \
    -exportOptionsPlist ExportOptions-AppStore.plist
```

## Step 5: App Store Connect Setup

1. Go to https://appstoreconnect.apple.com
2. Create New App:
   - Platform: iOS and macOS
   - Name: Quantum Terminal
   - Primary Language: English
   - Bundle ID: Select your bundle ID
   - SKU: quantum-terminal-001

3. Fill in App Information:
   - Category: Developer Tools
   - Content Rights: No third-party content
   - Age Rating: 4+

## Step 6: Prepare Marketing Materials

### Screenshots Required:
- **iPhone**: 1242 x 2688 (6.5") and 1242 x 2208 (5.5")
- **iPad**: 2048 x 2732 (12.9")
- **Mac**: 2880 x 1800 (16:10)

### App Preview Videos (optional):
- Show quantum particles
- Demonstrate terminal functionality
- Highlight interactive features

### Metadata:
```
Name: Quantum Terminal

Subtitle: Terminal with Quantum Physics

Description:
Experience the future of terminal emulators with Quantum Terminal - a revolutionary command-line interface that combines powerful terminal functionality with stunning quantum particle physics visualizations.

Features:
• Interactive 3D quantum particle effects
• Full terminal emulation with PTY support
• Mouse and touch interactions create particle bursts
• Easter egg commands trigger special effects
• Supports all standard terminal shortcuts
• Beautiful cyan and purple quantum color scheme
• Real-time physics simulation
• Cross-platform: works on Mac, iPhone, and iPad

Keywords:
terminal,console,quantum,physics,particles,3d,developer,programming,code,effects

Support URL: https://yourcompany.com/quantum-terminal/support
Marketing URL: https://yourcompany.com/quantum-terminal
```

## Step 7: Create Archive and Upload

### Using Xcode:
1. Select "Any iOS Device" or "Any Mac" as destination
2. Product → Archive
3. Window → Organizer
4. Select your archive → Distribute App
5. Choose "App Store Connect"
6. Upload

### Using command line:
```bash
# Upload to App Store Connect
xcrun altool --upload-app \
    -f build/QuantumTerminal.ipa \
    -t ios \
    -u your@email.com \
    -p @keychain:AC_PASSWORD
```

## Step 8: TestFlight Beta Testing

1. In App Store Connect → TestFlight
2. Add internal testers (your team)
3. Submit build for review
4. Once approved, add external testers

## Step 9: Submit for Review

1. Go to App Store Connect → App Store → iOS/macOS App
2. Create new version
3. Add build from TestFlight
4. Fill in "What's New"
5. Submit for Review

### Review Guidelines Compliance:
- ✅ No private APIs used
- ✅ No cryptocurrency mining
- ✅ Follows Human Interface Guidelines
- ✅ Includes age rating
- ✅ No offensive content

## Step 10: Post-Launch

1. Monitor reviews and ratings
2. Respond to user feedback
3. Regular updates with new features
4. Consider adding:
   - iCloud sync for settings
   - Shortcuts support
   - Widget for quick commands

## Automation Script

Create `deploy_appstore.sh`:

```bash
#!/bin/bash
set -e

# Configuration
TEAM_ID="YOUR_TEAM_ID"
BUNDLE_ID="com.yourcompany.quantum-terminal"
APP_NAME="QuantumTerminal"

echo "🚀 Building Quantum Terminal for App Store..."

# Clean
rm -rf build/
mkdir -p build

# Build and archive
echo "📦 Creating archives..."
xcodebuild -project $APP_NAME.xcodeproj \
           -scheme $APP_NAME \
           -sdk iphoneos \
           -configuration Release \
           -archivePath build/$APP_NAME-iOS.xcarchive \
           archive

xcodebuild -project $APP_NAME.xcodeproj \
           -scheme $APP_NAME \
           -sdk macosx \
           -configuration Release \
           -archivePath build/$APP_NAME-macOS.xcarchive \
           archive

# Export
echo "📤 Exporting for App Store..."
xcodebuild -exportArchive \
           -archivePath build/$APP_NAME-iOS.xcarchive \
           -exportOptionsPlist ios/ExportOptions.plist \
           -exportPath build/ios

xcodebuild -exportArchive \
           -archivePath build/$APP_NAME-macOS.xcarchive \
           -exportOptionsPlist macos/ExportOptions.plist \
           -exportPath build/macos

# Upload
echo "⬆️ Uploading to App Store Connect..."
xcrun altool --upload-app \
             -f build/ios/$APP_NAME.ipa \
             -type ios \
             --apiKey YOUR_API_KEY \
             --apiIssuer YOUR_ISSUER_ID

xcrun altool --upload-app \
             -f build/macos/$APP_NAME.pkg \
             -type macos \
             --apiKey YOUR_API_KEY \
             --apiIssuer YOUR_ISSUER_ID

echo "✅ Upload complete! Check App Store Connect for processing status."
```

## Tips for Success

1. **Performance**: Ensure 60 FPS on all devices
2. **Battery**: Optimize particle count on mobile
3. **Accessibility**: Add VoiceOver support
4. **Localization**: Consider adding more languages
5. **Reviews**: Respond professionally to all feedback

## Common Rejection Reasons to Avoid

- Missing privacy policy URL
- Crashes on launch
- Non-functional features
- Inappropriate content
- Performance issues
- Missing device support

## Pricing Strategy

Consider:
- Free with Pro upgrade ($4.99)
- One-time purchase ($2.99)
- Family Sharing enabled
- Educational discount

## Marketing

1. Create landing page
2. Make demo video
3. Reach out to tech blogs
4. Post on Product Hunt
5. Share on developer forums

Good luck with your App Store launch! 🚀