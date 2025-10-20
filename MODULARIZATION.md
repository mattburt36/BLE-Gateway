# Code Modularization - BLE Gateway v1.1.0

## Overview

The BLE Gateway codebase has been refactored from a single monolithic file into multiple modular header files, improving code organization, maintainability, and readability.

## File Structure

### Before
- `BLE-WiFi-Gateway.ino` - Single file with ~1330 lines containing all functionality

### After
- `BLE-WiFi-Gateway.ino` - Main file with setup(), loop() and global variable definitions (~224 lines)
- `config_manager.h` - Configuration storage and management
- `wifi_manager.h` - WiFi connection and configuration portal
- `ble_scanner.h` - BLE scanning and sensor data parsing
- `ota_manager.h` - Over-the-air firmware update handling
- `mqtt_handler.h` - MQTT/ThingsBoard communication

## Module Descriptions

### 1. config_manager.h
**Purpose:** Configuration storage, encryption, and remote config fallback

**Key Functions:**
- `encryptString()` / `decryptString()` - XOR encryption for credentials
- `saveConfig()` / `loadConfig()` - EEPROM read/write operations
- `fetchConfigFromURL()` - Remote configuration retrieval
- `initConfigManager()` - Initialize EEPROM

**Global Variables Used:**
- `wifi_ssid`, `wifi_password`
- `thingsboard_host`, `thingsboard_token`
- `config_url`, `config_username`, `config_password`

### 2. wifi_manager.h
**Purpose:** WiFi connectivity and web-based configuration portal

**Key Functions:**
- `printWiFiStatus()` - Display WiFi connection details
- `syncTime()` - NTP time synchronization
- `setupAP()` - Create access point for configuration
- `setupWebServer()` - Web UI for device configuration
- `tryWiFi()` - Attempt WiFi connection

**Global Variables Used:**
- `wifi_ssid`, `wifi_password`
- `wifi_connected`, `config_mode`, `time_synced`
- `server`, `dnsServer`

### 3. ble_scanner.h
**Purpose:** BLE device scanning and sensor data parsing

**Key Functions:**
- `toHex()` - Convert binary data to hex string
- `simpleHash()` - Data deduplication hashing
- `parseHoptechSensor()` - Parse Hoptech/MOKO L02S sensor data
- `parseMokoTH()` - Parse MOKO T&H sensor data
- `processAdvert()` - Process BLE advertisements
- `bleScanTask()` - FreeRTOS task for BLE scanning
- `initBLEScanner()` - Initialize BLE hardware
- `MyAdvertisedDeviceCallbacks` - BLE callback class

**Global Variables Used:**
- `pBLEScan`
- `detectionBuffer`, `detectionBufferMutex`
- `config_mode`, `otaState`

**Data Structures:**
- `BLEAdvertEntry` - Stores parsed advertisement data

### 4. ota_manager.h
**Purpose:** Over-the-air firmware update implementation

**Key Functions:**
- `performOTAUpdate()` - Download and flash new firmware

**Global Variables Used:**
- `otaState`
- `mqtt_connected`

**Data Structures:**
- `OTAState` - Firmware update status tracking

**Dependencies:**
- `sendOTAStatus()` - Defined in mqtt_handler.h
- `mqttClientLoop()` - Defined in mqtt_handler.h

### 5. mqtt_handler.h
**Purpose:** MQTT/ThingsBoard communication and gateway integration

**Key Functions:**
- `sendOTAStatus()` - Report firmware update progress
- `sendGatewayAttributes()` - Send device telemetry to ThingsBoard
- `mqttCallback()` - Handle incoming MQTT messages
- `tryMQTT()` - Establish MQTT connection
- `sendBatchToThingsBoardGateway()` - Batch send BLE device data
- `mqttMaintenanceTask()` - FreeRTOS task for MQTT keepalive
- `messageProcessingTask()` - FreeRTOS task for batch processing
- `mqttClientLoop()` - Helper for OTA manager
- `mqttClientPublish()` - Helper for OTA manager

**Global Variables Used:**
- `client`, `espClient`
- `mqtt_connected`, `wifi_connected`, `time_synced`
- `otaState`, `detectionBuffer`
- `mqttMutex`, `detectionBufferMutex`
- `mqttFailStart`, `apModeOffered`

**Dependencies:**
- `fetchConfigFromURL()` - Defined in config_manager.h
- `toHex()` - Defined in ble_scanner.h
- `performOTAUpdate()` - Defined in ota_manager.h

## Main File (BLE-WiFi-Gateway.ino)

**Purpose:** Application entry point and global state management

**Responsibilities:**
- Include all module headers
- Define all global variables (declared as `extern` in modules)
- Implement `setup()` function - initialization
- Implement `loop()` function - main event loop
- Create FreeRTOS tasks
- Handle WiFi loss and config mode transitions

**Key Global Variables:**
- Configuration: `wifi_ssid`, `wifi_password`, `thingsboard_host`, `thingsboard_token`
- State flags: `wifi_connected`, `mqtt_connected`, `config_mode`, `time_synced`
- Task handles: `mqttTaskHandle`, `bleTaskHandle`, `processingTaskHandle`
- Mutexes: `detectionBufferMutex`, `mqttMutex`
- Data structures: `detectionBuffer`, `otaState`
- Network clients: `espClient`, `client`, `server`, `dnsServer`
- BLE: `pBLEScan`

## Benefits of Modularization

1. **Improved Readability:** Each module focuses on a specific area of functionality
2. **Easier Maintenance:** Changes to one module don't affect others
3. **Better Organization:** Related functions are grouped together
4. **Simplified Testing:** Individual modules can be tested in isolation
5. **Code Reuse:** Modules can potentially be reused in other projects
6. **Clear Dependencies:** Module boundaries make dependencies explicit

## Module Dependencies

```
Main (.ino)
    ├── config_manager.h
    ├── wifi_manager.h → config_manager.h
    ├── ble_scanner.h
    ├── ota_manager.h → mqtt_handler.h
    └── mqtt_handler.h → config_manager.h, ble_scanner.h, ota_manager.h
```

## Compilation Notes

To compile this project:

1. Ensure all `.h` files are in the same directory as `BLE-WiFi-Gateway.ino`
2. Arduino IDE will automatically include headers from the sketch folder
3. All required libraries must still be installed (see README.md)
4. No changes to Arduino IDE settings or build process required

## Testing Checklist

After refactoring, verify:

- [ ] Code compiles without errors
- [ ] ESP32 boots successfully
- [ ] Configuration portal works
- [ ] WiFi connection succeeds
- [ ] MQTT connection succeeds
- [ ] BLE scanning detects devices
- [ ] Telemetry is published to ThingsBoard
- [ ] OTA updates work
- [ ] Config URL fallback works
- [ ] All FreeRTOS tasks start correctly
- [ ] No memory leaks or crashes

## Migration Notes

**For existing users:**
- No configuration changes required
- EEPROM data format unchanged
- All features work exactly as before
- Simply replace the old .ino file with the new files

**For developers:**
- When adding new features, determine which module it belongs to
- Keep cross-module dependencies minimal
- Use `extern` declarations for shared variables
- Maintain the single-responsibility principle per module

## Future Improvements

Potential enhancements to the modular structure:

1. Convert header files to .cpp/.h pairs for better encapsulation
2. Use classes/namespaces to avoid global variables
3. Implement dependency injection for better testability
4. Create abstract interfaces for hardware dependencies
5. Add unit tests for individual modules
6. Consider splitting large modules (mqtt_handler.h) further

---

**Document Version:** 1.0  
**Date:** 2025-10-20  
**Author:** GitHub Copilot Coding Agent
