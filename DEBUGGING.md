# Debugging Guide - No Serial Output

## Quick Checks

### 1. Find Your Serial Port

```bash
# List all USB serial devices
ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null

# Or check kernel messages for ESP32
sudo dmesg | grep -i "usb\|tty" | tail -20
```

Common ports:
- **Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`
- **Windows:** `COM3`, `COM4`, etc.
- **Mac:** `/dev/cu.usbserial-*`

### 2. Monitor Serial Output

```bash
# Option 1: PlatformIO (recommended)
cd /home/matt/src/Hoptech/Devices/BLE-Gateway
platformio device monitor --baud 115200

# Option 2: Screen
screen /dev/ttyUSB0 115200

# Option 3: Minicom
minicom -D /dev/ttyUSB0 -b 115200

# Option 4: Use debug helper script
./debug.sh
```

### 3. Upload Firmware and Monitor

```bash
platformio run --target upload --target monitor
```

---

## Common Issues

### Issue 1: No Serial Output at All

**Possible Causes:**
1. USB cable is data-only (not power+data)
2. Wrong baud rate
3. Device not in serial mode
4. USB drivers missing

**Solutions:**
```bash
# Check if device is detected
lsusb | grep -i "espressif\|cp210\|ch340\|ftdi"

# Check dmesg for connection
sudo dmesg | tail -30

# Try different baud rates
platformio device monitor --baud 9600
platformio device monitor --baud 115200

# Reset device while monitoring
# Press RESET button on XIAO ESP32-S3
```

### Issue 2: Garbled Output

**Problem:** Wrong baud rate

**Solution:**
```bash
# Our firmware uses 115200
platformio device monitor --baud 115200
```

### Issue 3: Device Not Detected

**Problem:** USB permissions or driver

**Solution:**
```bash
# Add user to dialout group (Linux)
sudo usermod -a -G dialout $USER
# Logout and login again

# Or use sudo temporarily
sudo platformio device monitor --baud 115200
```

### Issue 4: "Permission Denied" Error

**Solution:**
```bash
# Fix permissions
sudo chmod 666 /dev/ttyUSB0

# Or permanently with udev rule
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", MODE="0666"' | sudo tee /etc/udev/rules.d/99-esp32.rules
sudo udevadm control --reload-rules
```

---

## Expected Serial Output

When the device boots successfully, you should see:

```
========================================
BLE-Gateway-XIAO v2.0.0
XIAO ESP32-S3 BLE Gateway
========================================

Device ID: A1B2C3D4E5F6

Configuration manager initialized.
✗ No valid configuration found
No configuration found in flash.
Starting WiFi Access Point for configuration...

✓ Configuration portal started
  SSID: BLE-Gateway-Setup
  Password: 12345678
  IP: 192.168.4.1
Setup complete.
```

---

## Debugging Steps

### Step 1: Verify Upload Worked

```bash
cd /home/matt/src/Hoptech/Devices/BLE-Gateway

# Check firmware was built
ls -lh .pio/build/seeed_xiao_esp32s3/firmware.bin

# Should show ~1.4MB file
```

### Step 2: Upload with Verbose Output

```bash
platformio run --target upload -v
```

Look for:
- ✅ "Writing at 0x..." messages (firmware uploading)
- ✅ "Hash of data verified" (upload successful)
- ✅ "Leaving... Hard resetting via RTS pin..." (device resetting)

### Step 3: Monitor Immediately After Upload

```bash
# Upload and immediately start monitoring
platformio run --target upload --target monitor
```

### Step 4: Press Reset Button

If still no output:
1. Keep serial monitor open
2. Press **RESET** button on XIAO ESP32-S3
3. Watch for boot messages

### Step 5: Check Boot Mode

The XIAO ESP32-S3 might be in download mode:

**Press RESET button while monitoring** - this should boot the device normally

---

## Manual Reset Sequence

If device seems stuck:

1. **Disconnect USB**
2. **Reconnect USB**
3. **Immediately start monitoring:**
   ```bash
   platformio device monitor --baud 115200
   ```
4. **Press RESET button** (small button on board)

---

## Advanced Debugging

### Check Flash Contents

```bash
# Read firmware from device
esptool.py --chip esp32s3 --port /dev/ttyUSB0 read_flash 0x0 0x400000 flash_dump.bin

# Verify bootloader
esptool.py --chip esp32s3 --port /dev/ttyUSB0 read_flash 0x1000 0x7000 bootloader_dump.bin
```

### Erase and Reflash

```bash
# Complete erase (will remove all config)
esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash

# Upload fresh firmware
platformio run --target upload --target monitor
```

### Enable Verbose Boot Messages

Edit `platformio.ini` and add:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5  ; Maximum verbosity
    -DCONFIG_ARDUHAL_LOG_COLORS=1
```

Then rebuild and upload:
```bash
platformio run --target upload --target monitor
```

---

## Quick Test Commands

```bash
# Test 1: Check device connection
lsusb | grep -i esp

# Test 2: Check serial port
ls -la /dev/ttyUSB* /dev/ttyACM*

# Test 3: Quick monitor
platformio device monitor

# Test 4: Upload and monitor
platformio run -t upload -t monitor

# Test 5: List all detected devices
platformio device list
```

---

## Expected Behavior

### First Boot (No Config)
1. Device starts in **AP mode**
2. Creates WiFi network: `BLE-Gateway-Setup`
3. Serial shows: "Configuration portal started"
4. Web interface at: `192.168.4.1`

### After Configuration
1. Connects to WiFi
2. Syncs time via NTP
3. Fetches remote config
4. Connects to MQTTS
5. Starts BLE scanning
6. Serial shows: "All tasks created successfully!"

---

## Still No Output?

If you've tried everything above:

1. **Check the USB cable** - Some cables are power-only
2. **Try a different USB port** - Some ports have issues
3. **Check board LED** - Is it powered? (should see LED)
4. **Try Arduino IDE** - Upload a simple blink sketch to verify hardware
5. **Check XIAO ESP32-S3 drivers** - May need CH340 or CP210x drivers

---

## Get Help

Share this information:
```bash
# System info
uname -a
lsusb

# Serial devices
ls -la /dev/ttyUSB* /dev/ttyACM*

# Last kernel messages
sudo dmesg | tail -30

# PlatformIO device list
platformio device list

# Upload output
platformio run --target upload -v 2>&1 | tee upload.log
```

---

**Quick Start:**
```bash
cd /home/matt/src/Hoptech/Devices/BLE-Gateway
./debug.sh
# Choose option 1 or 2
```
