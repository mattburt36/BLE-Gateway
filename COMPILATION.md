# Compilation and Testing Guide

This guide helps you verify that the modularized code compiles and works correctly.

## Prerequisites

1. **Arduino IDE** (version 1.8.19 or newer recommended) or **Arduino CLI**
2. **ESP32 Board Support** installed in Arduino IDE
3. **Required Libraries** (see main README.md)

## File Checklist

Ensure all these files are in the same directory:

```
BLE-Gateway/
├── BLE-WiFi-Gateway.ino    (Main sketch - ~224 lines)
├── ble_scanner.h           (BLE functionality)
├── config_manager.h        (Configuration management)
├── mqtt_handler.h          (MQTT/ThingsBoard)
├── ota_manager.h           (OTA updates)
├── wifi_manager.h          (WiFi management)
├── README.md
├── MODULARIZATION.md
└── CHANGES.md
```

## Compilation Steps

### Option 1: Arduino IDE

1. **Open the sketch:**
   ```
   File → Open → BLE-WiFi-Gateway.ino
   ```

2. **Verify board settings:**
   - Board: "ESP32 Dev Module" (or your specific ESP32 board)
   - Upload Speed: 921600
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB (32Mb)
   - Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)

3. **Verify/Compile:**
   ```
   Sketch → Verify/Compile (Ctrl+R)
   ```

4. **Expected output:**
   ```
   Sketch uses XXXXX bytes (XX%) of program storage space.
   Global variables use XXXXX bytes (XX%) of dynamic memory.
   ```

### Option 2: Arduino CLI

1. **Install ESP32 core:**
   ```bash
   arduino-cli core install esp32:esp32
   ```

2. **Compile the sketch:**
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 BLE-WiFi-Gateway.ino
   ```

3. **Expected output:**
   ```
   Sketch uses XXXXX bytes (XX%) of program storage space.
   Used library    Version  Path
   WiFi            X.X.X    ...
   PubSubClient    X.X.X    ...
   (etc.)
   ```

## Common Compilation Errors and Solutions

### Error: "No such file or directory"

**Symptom:**
```
fatal error: config_manager.h: No such file or directory
```

**Solution:**
- Ensure all `.h` files are in the same directory as the `.ino` file
- Arduino IDE looks for headers in the sketch folder automatically

### Error: "'FIRMWARE_VERSION' was not declared"

**Symptom:**
```
error: 'FIRMWARE_VERSION' was not declared in this scope
```

**Solution:**
- Verify that `#define FIRMWARE_VERSION` is in the main `.ino` file BEFORE the module includes
- Check the include order - definitions must come before includes

### Error: Multiple definition errors

**Symptom:**
```
multiple definition of 'wifi_ssid'
```

**Solution:**
- This shouldn't happen with the current structure
- If it does, ensure variables are only defined once (in .ino) and declared as `extern` in headers
- Do not include `.h` files multiple times

### Error: Library not found

**Symptom:**
```
fatal error: PubSubClient.h: No such file or directory
```

**Solution:**
Install missing libraries via Arduino Library Manager:
- PubSubClient
- ArduinoJson (v7.x)
- ESP32 BLE Arduino

## Verification Checklist

After successful compilation:

- [ ] No compilation errors
- [ ] No warnings (or only minor warnings)
- [ ] Sketch size is reasonable (~900KB-1MB)
- [ ] Global variables usage is acceptable (<80% of RAM)

## Testing After Upload

### 1. Serial Monitor Test (115200 baud)

**Expected startup sequence:**
```
========================================
BLE Gateway Starting...
Firmware: BLE-Gateway v1.1.0
========================================

Config loaded, trying WiFi and MQTT...
...
WiFi connected!
==== WiFi Status ====
...
```

### 2. Configuration Portal Test

If no config exists:
```
No valid config, entering config mode...
AP IP: 192.168.4.1
Web server started for config mode
```

**Verify:**
1. WiFi AP "BLE-Gateway-Setup" appears
2. Can connect with password "12345678"
3. Navigate to http://192.168.4.1
4. Configuration form loads correctly

### 3. Module Functionality Tests

**BLE Scanner:**
```
Scanning BLE...

========== BLE Advertisement Detected ==========
MAC Address: XX:XX:XX:XX:XX:XX
RSSI: -XX dBm
...
```

**MQTT Handler:**
```
Connecting to MQTT (attempt 1/3)...connected!
Subscribed to:
  - v1/devices/me/attributes
  ...
Sending gateway attributes:
...
```

**WiFi Manager:**
```
Synchronizing time with NTP server...
Time synchronized successfully!
Current time: ...
```

**Task Creation:**
```
========== Starting FreeRTOS Tasks ==========
All tasks created successfully!
MQTT Maintenance Task started
BLE Scan Task started
Message Processing Task started
```

## Memory Usage Analysis

Check for reasonable memory usage:

```bash
# In Arduino IDE after compile:
Sketch uses 892524 bytes (68%) of program storage space. Maximum is 1310720 bytes.
Global variables use 37428 bytes (11%) of dynamic memory, leaving 290252 bytes for local variables. Maximum is 327680 bytes.
```

**Warning signs:**
- Flash usage >90% - May not have room for OTA updates
- RAM usage >70% - Risk of out-of-memory crashes

## Performance Checks

Monitor with Serial Monitor for 5-10 minutes:

1. **No crashes or resets:**
   - No "Guru Meditation Error"
   - No "Task watchdog" timeouts
   - No unexpected reboots

2. **Stable heap memory:**
   ```
   Free heap should stay >100KB
   ```

3. **Successful MQTT publishing:**
   ```
   ========== Sending Telemetry ==========
   ✓ Telemetry published successfully
   ```

## Regression Testing

Compare behavior with previous version:

| Feature | Works | Notes |
|---------|-------|-------|
| WiFi connection | ☐ | |
| MQTT connection | ☐ | |
| BLE scanning | ☐ | |
| Config portal | ☐ | |
| OTA updates | ☐ | |
| Telemetry publishing | ☐ | |
| Auto-reconnection | ☐ | |
| Time synchronization | ☐ | |

## Troubleshooting

### Issue: Compilation is slow

**Normal:**
- First compile: 1-3 minutes
- Subsequent compiles: 10-30 seconds

**If slower than this:**
- Close other applications
- Check available disk space
- Update Arduino IDE/CLI

### Issue: Upload fails

**Solutions:**
1. Press and hold BOOT button during upload
2. Check correct COM port selected
3. Try lower upload speed (115200)
4. Reset board before upload

### Issue: Code runs but behaves differently

**Debug steps:**
1. Check all `.h` files are latest version
2. Clear build cache (Arduino IDE → Preferences → Delete temp files)
3. Compare with git repository
4. Enable verbose Serial output

## Success Criteria

✅ Code compiles without errors  
✅ Upload succeeds  
✅ Device boots normally  
✅ All modules initialize  
✅ WiFi connects  
✅ MQTT connects  
✅ BLE scanning works  
✅ Telemetry publishes to ThingsBoard  
✅ No crashes for 24+ hours  
✅ Memory usage stable  

## Next Steps

After successful compilation and testing:

1. **Deploy to production:**
   - Upload to your devices
   - Monitor for 24-48 hours
   - Check ThingsBoard for data

2. **Create firmware binary for OTA:**
   ```
   Sketch → Export Compiled Binary
   ```
   Upload `BLE-WiFi-Gateway.ino.esp32.bin` to your OTA server

3. **Document any issues:**
   - Report bugs to repository
   - Share findings with maintainer

## Support

If you encounter issues:

1. Check MODULARIZATION.md for module details
2. Review TESTING.md for comprehensive test procedures
3. Compare with ARCHITECTURE.md for expected behavior
4. Report issues with:
   - Full compilation output
   - Serial monitor logs
   - Board details
   - Library versions

---

**Last Updated:** 2025-10-20  
**Applies to:** v1.1.0 (modularized)
