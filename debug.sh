#!/bin/bash
# Debug helper for BLE Gateway

echo "=========================================="
echo "BLE Gateway Debug Helper"
echo "=========================================="
echo ""

# Check if device is connected
echo "Looking for connected ESP32 devices..."
ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ No USB serial devices found!"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check USB cable is connected"
    echo "  2. Check device has power"
    echo "  3. Try: sudo dmesg | tail -20"
    echo ""
    exit 1
fi

echo ""
echo "Found serial device(s) above"
echo ""
echo "Select debug option:"
echo "  1) Monitor serial output (115200 baud)"
echo "  2) Upload firmware and monitor"
echo "  3) Erase flash and upload fresh"
echo "  4) Show last kernel messages (dmesg)"
echo ""
read -p "Enter choice (1-4): " choice

case $choice in
    1)
        echo "Starting serial monitor..."
        echo "Press Ctrl+C to exit"
        echo ""
        platformio device monitor --baud 115200
        ;;
    2)
        echo "Uploading firmware and monitoring..."
        platformio run --target upload --target monitor
        ;;
    3)
        echo "⚠️  This will ERASE ALL DATA on the device!"
        read -p "Are you sure? (yes/no): " confirm
        if [ "$confirm" = "yes" ]; then
            echo "Erasing flash..."
            esptool.py --chip esp32s3 erase_flash
            echo "Uploading firmware..."
            platformio run --target upload --target monitor
        fi
        ;;
    4)
        echo "Last 30 kernel messages:"
        echo ""
        sudo dmesg | tail -30
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac
