# Testing Guide for BLE Gateway v1.1.0

This guide will help you verify that all features are working correctly after upgrading to v1.1.0.

## Pre-requisites

- ESP32 board (Xiao Seeed ESP32-S3 or compatible)
- Arduino IDE with ESP32 board support
- Required libraries installed (see README)
- Access to ThingsBoard instance
- BLE sensors for testing (optional)

## Compilation Test

1. Open `BLE-WiFi-Gateway.ino` in Arduino IDE
2. Select your ESP32 board
3. Verify/Compile the sketch (Ctrl+R)
4. Check for compilation errors

**Expected Result:** Sketch compiles successfully without errors

## Upload and Basic Connectivity

1. Upload the sketch to your ESP32
2. Open Serial Monitor (115200 baud)
3. Reset the board

**Expected Output:**
```
========================================
BLE Gateway Starting...
Firmware: BLE-Gateway v1.1.0
========================================
```

## WiFi Configuration Test

### First Boot (No Config)

**Expected Behavior:**
1. Device creates WiFi AP named "BLE-Gateway-Setup"
2. Password: `12345678`
3. Serial output shows: "No valid config, entering config mode..."

**Steps:**
1. Connect to "BLE-Gateway-Setup" WiFi
2. Navigate to `192.168.4.1` in browser
3. Verify new UI appears with all fields:
   - WiFi Settings (SSID, Password)
   - MQTT Settings (Host, Token)
   - Config Fallback (URL, Username, Password)
4. Fill in configuration
5. Click "Save and Restart"

**Expected Result:** Device saves config and restarts

### Subsequent Boots (Config Exists)

**Expected Behavior:**
1. Device attempts WiFi connection
2. Serial output shows dots (.) while connecting
3. Successful connection message appears
4. NTP time sync begins

**Serial Output Example:**
```
Config loaded, trying WiFi and MQTT...
...........
WiFi connected!
==== WiFi Status ====
SSID: YourWiFi
IP Address: 192.168.1.100
Signal strength (RSSI): -45 dBm
WiFi Status: Connected
=====================
Synchronizing time with NTP server...
Time synchronized successfully!
Current time: Sun Jan 19 12:34:56 2025
```

## NTP Time Sync Test

**What to Check:**
1. After WiFi connection, look for NTP sync messages
2. Check if time is synchronized successfully

**Serial Output:**
```
Synchronizing time with NTP server...
..
Time synchronized successfully!
Current time: [timestamp]
```

**Expected Result:** Time sync completes within 10 seconds

## MQTT Connection Test

**What to Check:**
1. MQTT connection attempt after WiFi
2. Successful connection message
3. Subscription confirmations
4. Gateway attributes sent

**Serial Output:**
```
Connecting to MQTT (attempt 1/3)...connected!
Subscribed to:
  - v1/devices/me/attributes
  - v1/devices/me/attributes/response/+
  - v1/devices/me/rpc/request/+
Sending gateway attributes:
[JSON payload with device info]
```

**Expected Result:** MQTT connects on first attempt

## Config URL Fallback Test (Optional)

**Prerequisites:**
- Set up a web server with config JSON
- Configure URL in web portal

**Test Scenario:**
1. Configure gateway with INVALID MQTT credentials
2. Configure valid Config URL with correct MQTT credentials
3. Restart gateway

**Expected Behavior:**
1. Initial MQTT connection fails (3 attempts)
2. Gateway fetches config from URL
3. Gateway retries MQTT with new credentials
4. Connection succeeds

**Serial Output:**
```
Connecting to MQTT (attempt 3/3)...failed, rc=-2 try again in 2 seconds

MQTT connection failed, trying config URL fallback...
========== Fetching Config from URL ==========
Config URL: https://hoptech.co.nz/bgw-config/
Using authentication for config URL
Config received:
[JSON response]
Updated MQTT host: mqtt.thingsboard.cloud
Updated MQTT token
Config updated successfully from URL
Retrying MQTT with updated config...
Connecting to MQTT (attempt 1/3)...connected!
```

## Multi-threading Test

**What to Check:**
1. Tasks creation messages in Serial Monitor
2. Concurrent operation (BLE scans while MQTT publishes)

**Serial Output:**
```
========== Starting FreeRTOS Tasks ==========
All tasks created successfully!
==========================================

MQTT Maintenance Task started
BLE Scan Task started
Message Processing Task started
```

**Expected Result:** All three tasks start successfully

## BLE Scanning Test

**Prerequisites:** Have BLE sensors nearby

**What to Check:**
1. BLE scan messages every 10 seconds
2. Device detection messages
3. Parsed sensor data (if supported sensor)

**Serial Output:**
```
Scanning BLE...

========== BLE Advertisement Detected ==========
MAC Address: f0:74:bf:f0:63:5a
RSSI: -60 dBm
Name: Hoptech Sensor
Service Data present:
Service Data Length: 21 bytes
Service Data (hex): [hex data]
Service UUID: 0000ea01-0000-1000-8000-00805f9b34fb
>>> Hoptech/MOKO L02S Sensor Detected! <<<
  Temperature: 22.5°C
  Humidity: 45.3%
  Battery: 3000 mV
===============================================

Queued: f0:74:bf:f0:63:5a RSSI:-60 Name:Hoptech Sensor
```

## MQTT Publishing Test

**What to Check:**
1. Batch sending every 60 seconds
2. Device connection messages
3. Attributes published
4. Telemetry published

**Serial Output:**
```
Processing and sending batch data...

========== Connecting Devices ==========
[JSON with device list]
Device connect: ✓ OK

========== Sending Attributes ==========
[JSON with device attributes]
Attributes: ✓ OK

========== Sending Telemetry ==========
Devices in batch: 3
Payload length: 1234 bytes
✓ Telemetry published successfully
==========================================
```

## Gateway Telemetry Test

**What to Check in ThingsBoard:**
1. Gateway device shows online
2. Latest telemetry includes:
   - `chipTemperature`
   - `freeHeap`
   - `rssi`
   - `timestamp` (if time synced)
   - `timeSynced` (true/false)

## MQTT Reconnection Test

**Test Scenario:**
1. Gateway running normally with MQTT connected
2. Disconnect ThingsBoard server or block port 1883
3. Wait 30 seconds
4. Restore connection

**Expected Behavior:**
1. Gateway detects disconnection
2. Attempts reconnection every 30 seconds
3. Successfully reconnects when service is restored
4. Continues normal operation

**Serial Output:**
```
MQTT disconnected, attempting reconnect...
Connecting to MQTT (attempt 1/3)...failed, rc=-2 try again in 2 seconds
[30 seconds later]
MQTT disconnected, attempting reconnect...
Connecting to MQTT (attempt 1/3)...connected!
MQTT reconnected successfully!
```

## Thread Safety Test

**What to Monitor:**
- No crashes or watchdog resets
- No data corruption (mismatched sensor readings)
- Smooth concurrent operation

**Signs of Issues:**
- Task watchdog timeouts
- Guru meditation errors
- Inconsistent data in ThingsBoard

**Expected Result:** System runs stably for extended periods (hours/days)

## Credential Encryption Test

**Steps:**
1. Configure gateway with WiFi and MQTT credentials
2. Save configuration
3. Read EEPROM contents (optional: use EEPROM dump tool)
4. Verify passwords are not stored in plaintext

**Expected Result:** Passwords appear encrypted/obfuscated in EEPROM

## OTA Update Test

**Test via ThingsBoard:**
1. Upload new firmware binary to web server
2. Set shared attributes on gateway device:
```json
{
  "fw_title": "BLE-Gateway",
  "fw_version": "1.2.0",
  "fw_url": "https://your-server.com/firmware.bin"
}
```
3. Monitor Serial output

**Expected Behavior:**
1. Gateway receives update notification
2. Downloads firmware with progress updates
3. Flashes firmware
4. Reboots with new version

## Performance Metrics

**Healthy Operation Indicators:**
- Free heap memory: > 100KB
- MQTT messages publish successfully (100% success rate)
- BLE scans complete within 5 seconds
- No task watchdog timeouts
- WiFi signal strength: > -70 dBm for stable operation

## Common Issues and Solutions

### Issue: Compilation Errors

**Solution:** 
- Verify all libraries are installed
- Check ArduinoJson version (v7.x required)
- Update ESP32 board support

### Issue: Tasks Not Starting

**Solution:**
- Check free heap memory
- Reduce task stack sizes if needed
- Verify board has dual-core support

### Issue: MQTT Connection Fails Immediately

**Solution:**
- Verify ThingsBoard host is correct
- Check device token is valid
- Test network connectivity
- Try config URL fallback

### Issue: BLE Scans Find No Devices

**Solution:**
- Verify BLE sensors are powered and advertising
- Check antenna is connected (external antenna boards)
- Reduce distance between gateway and sensors
- Check Serial Monitor for scan activity

### Issue: Time Sync Fails

**Solution:**
- Verify UDP port 123 is not blocked
- Try different NTP servers
- Check network has internet access
- System will work without time sync

## Success Criteria

✅ All tests pass without errors
✅ Gateway operates stably for 24+ hours
✅ BLE devices detected and published to ThingsBoard
✅ MQTT connection remains stable
✅ Automatic reconnection works
✅ No memory leaks (stable free heap)
✅ No task watchdog timeouts or crashes

## Reporting Issues

If you encounter issues:
1. Capture complete Serial Monitor output
2. Note your board model and configuration
3. Document steps to reproduce
4. Check ThingsBoard logs for server-side errors
5. Report to repository maintainer

## Additional Notes

- Gateway creates tasks only when NOT in config mode
- Tasks are deleted when entering config mode
- MQTT maintenance task runs on Core 0 (higher priority)
- BLE scanning task runs on Core 1 (dedicated)
- Processing task runs on Core 0
- Mutexes ensure thread-safe access to shared data
