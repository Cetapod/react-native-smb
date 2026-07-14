#!/bin/bash

# npm-friendly XCFramework creation script
# Run from project root via: npm run build:xcframework
# Creates XCFramework with consistent binary names for @cetapod/react-native-smb

set -e

echo "🏗️ Creating XCFramework for @cetapod/react-native-smb..."

# Check if we're in the project root
if [[ ! -d "ios/libs" ]] || [[ ! -f "package.json" ]]; then
    echo "❌ Error: Not in project root or ios/libs not found"
    echo "   Please run this script from the project root: npm run build:xcframework"
    exit 1
fi

# Use current directory as project root
PROJECT_ROOT="$(pwd)"

# Check if the separate libraries exist
if [[ ! -f "ios/libs/device/libsmb2-device.a" ]]; then
    echo "❌ Device library not found. Please run the build script first: npm run build:libsmb2"
    exit 1
fi

if [[ ! -f "ios/libs/simulator/libsmb2-simulator.a" ]]; then
    echo "❌ Simulator library not found. Please run the build script first: npm run build:libsmb2"
    exit 1
fi

# Create temporary directory with consistent naming
temp_dir=$(mktemp -d)
echo "🔧 Working in temporary directory: $temp_dir"

# Copy libraries with consistent names
cp "ios/libs/device/libsmb2-device.a" "$temp_dir/libsmb2.a"
device_lib="$temp_dir/libsmb2.a"

# Copy simulator library with consistent name
cp "ios/libs/simulator/libsmb2-simulator.a" "$temp_dir/libsmb2_sim.a"
simulator_lib="$temp_dir/libsmb2_sim.a"

# Clean and create output directory
rm -rf "ios/libs/libsmb2.xcframework"
mkdir -p "ios/libs"

echo "📦 Creating XCFramework with consistent binary names..."

# Create XCFramework using xcodebuild with renamed libraries
xcodebuild -create-xcframework \
    -library "$device_lib" \
    -headers "ios/include" \
    -library "$simulator_lib" \
    -headers "ios/include" \
    -output "ios/libs/libsmb2.xcframework"

# Rename the simulator library inside the framework to match device library name
mv "ios/libs/libsmb2.xcframework/ios-arm64_x86_64-simulator/libsmb2_sim.a" \
   "ios/libs/libsmb2.xcframework/ios-arm64_x86_64-simulator/libsmb2.a"

# Update the Info.plist to reflect the consistent binary name
sed -i '' 's/libsmb2_sim\.a/libsmb2.a/g' "ios/libs/libsmb2.xcframework/Info.plist"

# Clean up temp directory
rm -rf "$temp_dir"

echo "✅ XCFramework created successfully with consistent binary names!"
echo ""
echo "🔍 Verifying binary names:"
find "ios/libs/libsmb2.xcframework" -name "*.a" -exec basename {} \;
echo ""
echo "📁 XCFramework structure:"
find "ios/libs/libsmb2.xcframework" -type f | head -10

echo ""
echo "🎉 XCFramework ready at: ios/libs/libsmb2.xcframework"
echo ""
echo "All libraries now have the consistent name 'libsmb2.a'"
echo "This should resolve the CocoaPods installation error."
echo ""
echo "🚀 Next steps:"
echo "1. cd example && pod install"
echo "2. Build your Expo project"
