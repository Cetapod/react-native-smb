#!/bin/bash

# npm-friendly iOS libsmb2 build script
# Run from project root via: npm run build:libsmb2
# Creates separate device and simulator libraries

set -e

echo "📱 Building libsmb2 for iOS - @cetapod/react-native-smb"

# Check if we're in the correct directory (project root)
if [[ ! -d "libsmb2" ]] || [[ ! -f "libsmb2/CMakeLists.txt" ]]; then
    echo "❌ Error: libsmb2 submodule not found or not in project root"
    echo "   Please run this script from the project root: npm run build:libsmb2"
    exit 1
fi

# Get project root (current directory when run from npm script)
PROJECT_ROOT="$(pwd)"

# Change to libsmb2 directory for building
cd libsmb2

# Clean previous builds
echo "🧹 Cleaning previous builds..."
rm -rf build
rm -rf "${PROJECT_ROOT}/ios/libs"

# Create build structure  
mkdir -p build

# Create output structure
mkdir -p "${PROJECT_ROOT}/ios/libs/device"
mkdir -p "${PROJECT_ROOT}/ios/libs/simulator"

# Get SDK paths and versions
DEVICE_SDK_PATH=$(xcrun --sdk iphoneos --show-sdk-path)
SIMULATOR_SDK_PATH=$(xcrun --sdk iphonesimulator --show-sdk-path)
MIN_IOS_VERSION="12.0"

echo "📱 Device SDK: $DEVICE_SDK_PATH"
echo "🖥️  Simulator SDK: $SIMULATOR_SDK_PATH"
echo "📱 Minimum iOS version: $MIN_IOS_VERSION"

# Function to build for a specific target
build_target() {
    local target_name=$1
    local build_dir=$2
    local sdk_path=$3
    local architectures=$4
    local output_name=$5
    local output_dir=$6
    
    echo ""
    echo "🔨 Building $target_name ($architectures)..."
    
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Configure with proper platform settings
    cmake ../.. \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT="$sdk_path" \
        -DCMAKE_OSX_ARCHITECTURES="$architectures" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_IOS_VERSION" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DWITHOUT_LIBKRB5=ON \
        -DENABLE_WERROR=OFF \
        -DENABLE_EXAMPLES=OFF \
        -DCMAKE_C_FLAGS="-fembed-bitcode=off"
    
    # Use Apple config if available
    if [[ -f "../../include/apple/config.h" ]]; then
        echo "🍎 Using Apple-specific configuration..."
        cp ../../include/apple/config.h .
    fi
    
    # Build
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    # Copy to output location with proper naming
    cp lib/libsmb2.a "${PROJECT_ROOT}/ios/libs/${output_dir}/${output_name}"
    
    # Verify the platform
    echo "🔍 Verifying $target_name platform:"
    echo "Library info:"
    file "${PROJECT_ROOT}/ios/libs/${output_dir}/${output_name}"
    lipo -info "${PROJECT_ROOT}/ios/libs/${output_dir}/${output_name}"
    
    # Check platform of object files
    echo "Platform verification:"
    ar -x "${PROJECT_ROOT}/ios/libs/${output_dir}/${output_name}" aes.c.o 2>/dev/null || echo "No aes.c.o found"
    if [[ -f "aes.c.o" ]]; then
        platform_info=$(otool -l aes.c.o | grep -A 3 "platform" | head -4)
        echo "$platform_info"
        rm -f aes.c.o
    fi
    
    cd ../..
    echo "✅ $target_name build completed"
}

echo ""
echo "=== Building iOS Device Library ==="
# Build iOS Device library (arm64 only) - Platform 2 = iOS
build_target "iOS Device" "build/ios-device" "$DEVICE_SDK_PATH" "arm64" "libsmb2-device.a" "device"

echo ""
echo "=== Building iOS Simulator Library ==="
# Build iOS Simulator library (x86_64 + arm64 for Apple Silicon) - Platform 7 = iOS Simulator
build_target "iOS Simulator" "build/ios-simulator" "$SIMULATOR_SDK_PATH" "x86_64;arm64" "libsmb2-simulator.a" "simulator"

# Copy headers to include directory
echo ""
echo "📄 Copying headers to include directory..."
mkdir -p "${PROJECT_ROOT}/ios/include"
cp -r include/smb2 "${PROJECT_ROOT}/ios/include/"

# Return to project root
cd "${PROJECT_ROOT}"

echo ""
echo "🔍 Final verification:"
echo ""
echo "Device library (iOS Physical Devices):"
file "ios/libs/device/libsmb2-device.a"
lipo -info "ios/libs/device/libsmb2-device.a"

echo ""
echo "Simulator library (iOS Simulator):"
file "ios/libs/simulator/libsmb2-simulator.a"
lipo -info "ios/libs/simulator/libsmb2-simulator.a"

echo ""
echo "🎉 Build completed successfully!"
echo ""
echo "📍 Libraries created:"
echo "  Device:     ios/libs/device/libsmb2-device.a"
echo "  Simulator:  ios/libs/simulator/libsmb2-simulator.a"
echo "  Headers:    ios/include/smb2/"
echo ""
echo "🚀 Ready to create XCFramework! Run: npm run build:xcframework"
