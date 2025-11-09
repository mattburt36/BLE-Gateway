#!/bin/bash
# Build and validate script for BLE Gateway

echo "========================================="
echo "BLE Gateway v2.0.0 - Build Script"
echo "========================================="
echo ""

# Check if PlatformIO is installed
if ! command -v platformio &> /dev/null; then
    echo "❌ PlatformIO not found!"
    echo ""
    echo "Install with:"
    echo "  pip install platformio"
    echo ""
    exit 1
fi

echo "✓ PlatformIO found"
echo ""

# Check project structure
echo "Checking project structure..."
files=(
    "platformio.ini"
    "src/main.cpp"
    "src/config_manager.h"
    "src/wifi_manager.h"
    "src/mqtt_handler.h"
    "src/ble_scanner.h"
    "src/device_tracker.h"
)

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ❌ $file missing!"
        exit 1
    fi
done

echo ""
echo "Building project..."
echo ""

# Build the project
platformio run

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================="
    echo "✓ Build successful!"
    echo "========================================="
    echo ""
    echo "Next steps:"
    echo "  1. Upload: platformio run --target upload"
    echo "  2. Monitor: platformio device monitor"
    echo ""
else
    echo ""
    echo "========================================="
    echo "❌ Build failed!"
    echo "========================================="
    echo ""
    exit 1
fi
