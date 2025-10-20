# Changes Summary - v1.1.0

## Overview

This release addresses the reported MQTT connection stability issues and implements the requested multi-threaded architecture with improved reliability features. Additionally, the codebase has been refactored into a modular structure for better maintainability.

## Modularization (2025-10-20) ✅

**Issue:** "could you now make this into multiple code files relative to it's related modules? IE ble scanning in one file, mqtt handling in one file, wifi handling in another"

**Solution:** Refactored the monolithic ~1330-line file into 6 modular files:

1. **config_manager.h** - EEPROM storage, encryption, config URL fallback
2. **wifi_manager.h** - WiFi connection, NTP sync, configuration portal
3. **ble_scanner.h** - BLE scanning, sensor parsing, advertisement processing
4. **ota_manager.h** - Over-the-air firmware update implementation
5. **mqtt_handler.h** - MQTT/ThingsBoard gateway operations, FreeRTOS tasks
6. **BLE-WiFi-Gateway.ino** - Main file (setup, loop, global variables)

**Benefits:**
- Each module has a single, clear responsibility
- Easier to locate and modify specific functionality
- Better code organization and readability
- Simplified maintenance and testing
- Clear separation of concerns

See [MODULARIZATION.md](MODULARIZATION.md) for detailed module documentation.

## Problem Statement Addressed

**Original Issue:** "There is a bug with this code, where I'm seeing it drop a connection and need to be re-setup, when the mqtt instance is definitely still there."

**Solution:** Implemented a dedicated MQTT maintenance task with improved reconnection logic, keepalive management, and automatic recovery.

## Major Changes

### 1. Multi-threaded Architecture ✅

Implemented FreeRTOS-based task system:

- **MQTT Maintenance Task (Core 0, Priority 2)**
  - Runs independently to maintain MQTT connection
  - Automatic reconnection every 30 seconds if disconnected
  - Handles keepalive messages
  - Processes incoming MQTT messages and RPC commands
  - Periodic gateway status updates (every 5 minutes)

- **BLE Scanning Task (Core 1, Priority 1)**
  - Dedicated task for Bluetooth scanning
  - Runs on separate core for optimal performance
  - Continuous scanning without blocking other operations
  - Scans every 10 seconds

- **Message Processing Task (Core 0, Priority 1)**
  - Batches and processes BLE device data
  - Sends telemetry to ThingsBoard every 60 seconds
  - Thread-safe data handling with mutexes

**Benefits:**
- No more blocking operations
- MQTT connection maintained independently
- Better CPU utilization
- More reliable operation

### 2. Improved MQTT Reconnection Logic ✅

**Previous Behavior:**
- Simple reconnection attempts
- Would enter AP mode after 5 minutes of failure
- No automatic config refresh

**New Behavior:**
- Dedicated task monitors connection status
- Automatic reconnection every 30 seconds
- Unique client ID per device (prevents conflicts)
- Config URL fallback before entering AP mode
- Better error handling and state management
- Thread-safe MQTT operations

**Key Code Changes:**
```cpp
// Unique client ID prevents connection conflicts
String clientId = "BLEGateway-" + WiFi.macAddress();
clientId.replace(":", "");

// Automatic fallback to config URL if MQTT fails
if (attempts >= 3 && config_url.length() > 0) {
    fetchConfigFromURL(); // Try to get updated credentials
}
```

### 3. NTP Time Synchronization ✅

**Implementation:**
- Automatic time sync on WiFi connection
- Uses `pool.ntp.org` and `time.nist.gov`
- Includes timestamp in gateway telemetry
- System continues to operate even if time sync fails

**New Functions:**
- `syncTime()` - Synchronizes with NTP servers
- Adds `timestamp` and `timeSynced` to gateway attributes

### 4. Config URL Fallback ✅

**Purpose:** Automatically fetch MQTT credentials from remote server if connection fails

**Implementation:**
- Configurable URL (default: https://hoptech.co.nz/bgw-config/)
- Supports basic authentication
- Sends device identification headers
- Updates MQTT config automatically
- Retries connection with new credentials

**New Functions:**
- `fetchConfigFromURL()` - Retrieves config from remote server

**Web Portal Fields:**
- Config URL (optional)
- Config Username (optional)
- Config Password (optional, encrypted)

### 5. Credential Encryption ✅

**Previous Behavior:**
- Credentials stored in plaintext in EEPROM

**New Behavior:**
- Passwords encrypted before storage
- XOR encryption with device-specific key
- Symmetric encryption for easy retrieval

**New Functions:**
- `encryptString()` - Encrypts sensitive data
- `decryptString()` - Decrypts stored data

**Encrypted Fields:**
- WiFi password
- MQTT device token
- Config server password

### 6. Enhanced Gateway Telemetry ✅

**New Attributes:**
- `chipTemperature` - ESP32 internal temperature
- `timeSynced` - NTP sync status
- `timestamp` - Current Unix timestamp

**Benefits:**
- Better monitoring of gateway health
- Temperature-based alerts possible
- Accurate timestamping for debugging

### 7. Improved Web Configuration Portal ✅

**UI Improvements:**
- Better visual design with CSS styling
- Organized sections (WiFi, MQTT, Config Fallback)
- Field descriptions and placeholders
- Responsive design for mobile devices
- Visual feedback on save

**New Features:**
- Config URL fallback settings
- Password encryption notice
- Device info display (MAC address, firmware version)

### 8. Thread Safety ✅

**Mutex Protection:**
- `detectionBufferMutex` - Protects BLE device buffer
- `mqttMutex` - Protects MQTT client operations

**Thread-safe Operations:**
- BLE device detection and storage
- MQTT publishing and subscribing
- Batch data processing
- OTA updates

### 9. Better Code Organization ✅

**Structural Improvements:**
- Clear separation of concerns (tasks)
- Modular functions
- Better error handling
- Comprehensive comments
- Consistent naming conventions

## Breaking Changes

### ⚠️ EEPROM Layout Changed

**Impact:** Devices upgrading from v1.0.0 may need reconfiguration

**Reason:** Added new fields for config URL, username, and password

**Migration:** 
- Existing WiFi and MQTT settings should be preserved
- New optional fields will be empty (defaults apply)
- Consider entering config mode to set new optional fields

### ⚠️ Increased Memory Usage

**Impact:** Requires more heap memory due to FreeRTOS tasks

**Stack Sizes:**
- MQTT Task: 8KB
- BLE Task: 8KB
- Processing Task: 8KB
- Total: ~24KB additional stack space

**Recommendation:** Monitor `freeHeap` attribute to ensure sufficient memory

## Configuration Changes

### New EEPROM Addresses
- `CONFIG_URL_ADDR` (320) - Config fallback URL
- `CONFIG_USER_ADDR` (448) - Config username
- `CONFIG_PASS_ADDR` (512) - Config password (encrypted)

### New Constants
- `CONFIG_FALLBACK_URL` - Default config server URL
- `NTP_SERVER` - Primary NTP server
- `NTP_SERVER_BACKUP` - Backup NTP server

## Testing Recommendations

1. **MQTT Stability Test**
   - Run for 24+ hours
   - Monitor for disconnections
   - Verify automatic reconnection works
   - Check for memory leaks (stable freeHeap)

2. **Multi-threading Test**
   - Verify all tasks start successfully
   - Monitor for task watchdog timeouts
   - Check concurrent operations work smoothly

3. **Config Fallback Test**
   - Set invalid MQTT credentials
   - Configure valid config URL
   - Verify automatic credential retrieval

4. **Thread Safety Test**
   - Run with multiple BLE devices
   - Monitor for data corruption
   - Check for race conditions

5. **Load Test**
   - Test with 10+ BLE devices
   - Verify batch sending works
   - Monitor performance metrics

## Migration Guide

### From v1.0.0 to v1.1.0

1. **Backup your configuration** (note WiFi and MQTT settings)
2. **Upload v1.1.0 firmware**
3. **Verify automatic configuration preservation**
4. **Optionally configure new features:**
   - Enter config mode
   - Set Config URL (optional)
   - Set Config credentials (optional)
5. **Monitor Serial output for task startup**
6. **Verify MQTT connection stability**

## Performance Improvements

- **Better CPU utilization** - Dual-core usage optimized
- **Reduced blocking** - Non-blocking operations throughout
- **Improved responsiveness** - Independent task execution
- **More reliable MQTT** - Dedicated maintenance task
- **Faster recovery** - Automatic reconnection in 30 seconds

## Known Limitations

1. **XOR Encryption:** Basic encryption used for credentials. For production, consider implementing AES encryption.

2. **Config URL Security:** HTTPS recommended but certificate validation not implemented. Consider adding certificate pinning for production.

3. **Task Stack Size:** Fixed at 8KB per task. May need adjustment for complex operations.

4. **Single MQTT Connection:** Only one MQTT broker supported at a time.

## Future Enhancements

Consider for next version:
- [ ] AES-256 encryption for credentials
- [ ] MQTT over TLS/SSL
- [ ] Certificate validation for HTTPS
- [ ] Dynamic task stack sizing
- [ ] Local data caching during outages
- [ ] Web-based dashboard
- [ ] Multiple MQTT broker support
- [ ] Configurable task priorities via web portal

## Rollback Instructions

If issues occur with v1.1.0:

1. Re-flash v1.0.0 firmware
2. Configuration will be preserved (WiFi/MQTT)
3. New features will be ignored
4. System will operate in single-threaded mode

## Support

For issues or questions:
- Check `TESTING.md` for comprehensive testing guide
- Review Serial Monitor output at 115200 baud
- Check ThingsBoard logs for server-side issues
- Report issues to repository maintainer

## Credits

**Changes implemented by:** GitHub Copilot Coding Agent
**Requested by:** Matt Burton (@mattburt36)
**Date:** January 19, 2025
**Version:** 1.1.0
