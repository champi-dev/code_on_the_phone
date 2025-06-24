#!/bin/bash

echo "🚀 Building Quantum Terminal APK for Android"
echo "=========================================="

# Create a complete Flutter project structure
cd frontend

# Create necessary Flutter files
cat > android/app/src/main/AndroidManifest.xml << 'EOF'
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.quantum.terminal">
    
    <uses-permission android:name="android.permission.INTERNET" />
    
    <application
        android:label="Quantum Terminal"
        android:name="${applicationName}"
        android:icon="@mipmap/ic_launcher">
        <activity
            android:name=".MainActivity"
            android:exported="true"
            android:launchMode="singleTop"
            android:theme="@style/LaunchTheme"
            android:configChanges="orientation|keyboardHidden|keyboard|screenSize|smallestScreenSize|locale|layoutDirection|fontScale|screenLayout|density|uiMode"
            android:hardwareAccelerated="true"
            android:windowSoftInputMode="adjustResize">
            <meta-data
              android:name="io.flutter.embedding.android.NormalTheme"
              android:resource="@style/NormalTheme"
              />
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
        <meta-data
            android:name="flutterEmbedding"
            android:value="2" />
    </application>
</manifest>
EOF

# Create minimal Flutter project files
cat > .metadata << 'EOF'
version:
  revision: "latest"
  channel: "stable"
project_type: app
migration:
  platforms:
    - platform: android
      create_revision: latest
      base_revision: latest
EOF

cat > analysis_options.yaml << 'EOF'
include: package:flutter_lints/flutter.yaml
linter:
  rules:
    prefer_const_constructors: false
    prefer_const_literals_to_create_immutables: false
EOF

# Update pubspec.yaml with Android-specific config
cat >> pubspec.yaml << 'EOF'

flutter:
  uses-material-design: true
  
  assets:
    - assets/

  fonts:
    - family: JetBrainsMono
      fonts:
        - asset: fonts/JetBrainsMono-Regular.ttf
EOF

# Create a Dockerfile for building without Flutter installed
cat > Dockerfile.build << 'EOF'
FROM ghcr.io/cirruslabs/flutter:stable

WORKDIR /app

# Copy project files
COPY . .

# Get dependencies
RUN flutter pub get

# Build APK with custom backend URL
ENV BACKEND_URL="ws://10.0.2.2:8080/ws"
RUN flutter build apk --release --dart-define=BACKEND_URL=$BACKEND_URL

# The APK will be at build/app/outputs/flutter-apk/app-release.apk
EOF

echo "📱 Building APK using Docker..."
docker build -f Dockerfile.build -t quantum-terminal-builder .
docker run --rm -v $(pwd)/build:/app/build quantum-terminal-builder

echo "✅ APK built successfully!"
echo "📍 Location: frontend/build/app/outputs/flutter-apk/app-release.apk"
echo ""
echo "To install on your phone:"
echo "1. Enable 'Unknown Sources' in Android settings"
echo "2. Transfer the APK to your phone"
echo "3. Install and run!"
echo ""
echo "Note: Make sure your backend is accessible from your phone's network"