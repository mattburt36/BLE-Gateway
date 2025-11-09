# BLE Gateway for ThingsBoard

ESP32-based BLE to MQTT gateway for MOKO temperature/humidity sensors with multi-threaded operation, automatic reconnection, and OTA firmware updates.

## Features

- 🔍 **BLE Scanning** - Continuous scanning and advertisement parsing
- 📡 **MQTT Gateway** - ThingsBoard integration with auto-reconnect
- 🌡️ **Sensor Support** - MOKO L02S, MOKO T&H series
- 🔄 **OTA Updates** - Remote firmware updates via ThingsBoard
- 📶 **WiFi Portal** - Web-based configuration interface
- 🧵 **Multi-threaded** - FreeRTOS tasks for stability
- 🔐 **Encrypted Storage** - Secure credential storage
- ⏰ **Time Sync** - NTP time synchronization
- 🔁 **Config Fallback** - Remote config URL support
- 🎯 **Smart Batching** - Deduplication and batch telemetry

## Hardware Requirements

- ESP32 DevKit (ESP32-S3 or compatible)
- MOKO L02S or MOKO T&H BLE sensors

## Supported Sensors

### Hoptech/MOKO L02S-EA01
- Service UUID: `0000ea01-0000-1000-8000-00805f9b34fb`
- Temperature, Humidity, Battery voltage

### MOKO T&H Series  
- Service UUID: `0xABFE` (frame type: `0x70`)
- Temperature, Humidity, Battery voltage
- Device type detection (3-axis, T&H, combined)

## Quick Start

### Installation with PlatformIO (Recommended)

1. **Install Visual Studio Code** with the PlatformIO extension
2. **Open the project:**
   ```bash
   cd BLE-Gateway
   code .
   ```
3. **Build and upload:**
   - Click the PlatformIO icon in the sidebar
   - Click "Upload" or press Ctrl+Alt+U
   - Dependencies are installed automatically

### Installation with Arduino IDE (Alternative)

1. **Install Arduino IDE** with ESP32 board support
2. **Install required libraries** via Library Manager:
   - PubSubClient (MQTT)
   - ArduinoJson (v7.x)
   - ESP32 BLE Arduino
   - WiFi, HTTPClient, Update, EEPROM, WebServer, DNSServer (built-in)

3. **Open project:**
   ```
   File → Open → src/main.cpp
   ```

4. **Configure board:**
   - Board: ESP32 Dev Module
   - Upload Speed: 921600
   - Flash Size: 4MB
   - Partition Scheme: Default 4MB with spiffs

5. **Upload** to ESP32

### First-Time Configuration

1. After upload, ESP32 creates WiFi AP: `BLE-Gateway-Setup`
2. Connect to it (password: `12345678`)
3. Navigate to `192.168.4.1`
4. Enter your settings:
   - **WiFi SSID & Password**
   - **ThingsBoard MQTT Host** (e.g., `mqtt.thingsboard.cloud`)
   - **Device Access Token** from ThingsBoard
   - **Config URL** (optional) - for automatic credential fallback
5. Click "Save and Restart"

The gateway will connect and start scanning for BLE devices.

## Architecture

The gateway uses a modular, multi-threaded architecture for reliability:

### Code Structure

The project is organized into focused modules:
- `src/main.cpp` - Main application (setup, loop, globals)
- `src/config_manager.h` - Configuration storage and encryption
- `src/wifi_manager.h` - WiFi and configuration portal
- `src/ble_scanner.h` - BLE scanning and sensor parsing
- `src/ota_manager.h` - Firmware update handling
- `src/mqtt_handler.h` - MQTT/ThingsBoard integration
- `platformio.ini` - PlatformIO project configuration

### Multi-Threaded Operation

Three FreeRTOS tasks run concurrently:

1. **MQTT Maintenance Task** (Core 0, Priority 2)
   - Maintains MQTT connection with automatic reconnection
   - Processes incoming messages and RPC commands
   - Sends periodic gateway status updates
   - Handles OTA firmware updates

2. **BLE Scanning Task** (Core 1, Priority 1)
   - Continuously scans for BLE advertisements
   - Runs on dedicated core for optimal performance
   - Scans every 10 seconds with 5-second windows
   - Parses sensor data and updates shared buffer

3. **Message Processing Task** (Core 0, Priority 1)
   - Batches detected BLE devices
   - Sends telemetry to ThingsBoard every 60 seconds
   - Thread-safe access via mutex protection

### Data Flow

```
BLE Sensors → BLE Scan Task → Detection Buffer → Processing Task → MQTT → ThingsBoard
                                                                      ↓
                                                                 OTA Updates
                                                                      ↓
                                                               Config Fallback
                                                                      ↓
                                                                  NTP Sync
```

For detailed architecture documentation, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Configuration

### Web Portal Settings

Access the configuration portal at `192.168.4.1` when in AP mode:

- **WiFi Settings**
  - SSID: Your WiFi network name
  - Password: WiFi password (encrypted before storage)

- **MQTT Settings**
  - Host: ThingsBoard MQTT broker (e.g., `mqtt.thingsboard.cloud`)
  - Access Token: Device token from ThingsBoard (encrypted before storage)

- **Config Fallback (Optional)**
  - Config URL: Remote config server URL
  - Username: Authentication username (optional)
  - Password: Authentication password (encrypted before storage)

All credentials are encrypted before being stored in EEPROM.

### Config URL Fallback

If MQTT connection fails, the gateway can automatically fetch updated configuration from a remote server. The config URL should return JSON:

```json
{
  "mqtt_host": "mqtt.thingsboard.cloud",
  "mqtt_token": "your-device-token"
}
```

### Adjustable Parameters

Edit these in the code to customize behavior:

```cpp
// BLE scanning
const int SCAN_TIME = 5;  // BLE scan duration (seconds)
// Scan interval: 10 seconds between scans

// Batch sending
const unsigned long BATCH_INTERVAL = 60000;  // 60 seconds

// MQTT reconnection
const unsigned long MQTT_FAIL_AP_TIMEOUT = 300000;  // 5 minutes

// Firmware version
#define FIRMWARE_VERSION "1.1.0"
#define FIRMWARE_TITLE "BLE-Gateway"
```

## OTA Firmware Updates

The gateway supports remote firmware updates via ThingsBoard.

### Method 1: Shared Attributes

Set these shared attributes on your gateway device:

```json
{
  "fw_title": "BLE-Gateway",
  "fw_version": "1.2.0",
  "fw_url": "https://your-server.com/firmware.bin",
  "fw_size": 920000,
  "fw_checksum": "sha256_hash_optional"
}
```

### Method 2: RPC Call

Send an RPC command:

```json
{
  "method": "updateFirmware",
  "params": {
    "fw_title": "BLE-Gateway",
    "fw_version": "1.2.0",
    "fw_url": "https://your-server.com/firmware.bin",
    "fw_size": 920000
  }
}
```

### OTA Status Reporting

The gateway reports firmware status via device attributes:
- `current_fw_title`: Current firmware name
- `current_fw_version`: Currently running version
- `fw_state`: Update status (IDLE, DOWNLOADING, UPDATING, UPDATED, FAILED, UP_TO_DATE)
- `fw_progress`: Progress percentage (0-100)

### Other RPC Commands

```json
// Get current firmware information
{ "method": "getCurrentFirmware" }

// Reboot the gateway
{ "method": "reboot" }
```

### Creating Firmware Binary

**With PlatformIO:**
```bash
pio run -t buildfs  # Build filesystem
pio run             # Build firmware
```
Binary will be in `.pio/build/esp32dev/firmware.bin`

**With Arduino IDE:**
1. Sketch → Export compiled Binary
2. Binary will be saved as `main.ino.esp32.bin`
3. Upload to your web server for OTA updates

## ThingsBoard Integration

### Device Types

The gateway automatically assigns device types based on detected sensor format:
- `L02S` - Hoptech/MOKO L02S sensors
- `MOKO_TH` - MOKO T&H sensors
- `MOKO_3AXIS` - MOKO 3-axis accelerometer sensors
- `MOKO_3AXIS_TH` - MOKO combined sensors (3-axis + T&H)
- `BLE_SENSOR` - Generic BLE sensors with parsed data
- `BLE_BEACON` - Unknown/unparsed BLE devices

### Telemetry Data

Data sent for each BLE device:

| Key | Type | Description |
|-----|------|-------------|
| `temperature` | float | Temperature in °C (if available) |
| `humidity` | float | Relative humidity in % (if available) |
| `battery_mv` | integer | Battery voltage in millivolts (if available) |
| `rssi` | integer | Signal strength in dBm |
| `name` | string | BLE device name (if advertised) |
| `manufacturerData` | string | Hex-encoded manufacturer data (if present) |
| `serviceData` | string | Hex-encoded service data (if present) |

### Device Attributes

Attributes sent for each BLE device:

| Key | Type | Description |
|-----|------|-------------|
| `macAddress` | string | BLE MAC address |
| `deviceName` | string | BLE advertised name |
| `sensorType` | string | Detected sensor type |
| `hasTemperature` | boolean | Supports temperature readings |
| `hasHumidity` | boolean | Supports humidity readings |
| `hasBattery` | boolean | Reports battery voltage |

### Gateway Attributes

The gateway reports its own status:

| Key | Type | Description |
|-----|------|-------------|
| `current_fw_title` | string | Firmware name |
| `current_fw_version` | string | Current firmware version |
| `fw_state` | string | OTA update status |
| `chipModel` | string | ESP32 chip model |
| `chipRevision` | integer | ESP32 chip revision |
| `cpuFreqMHz` | integer | CPU frequency |
| `flashSize` | integer | Flash size in bytes |
| `freeHeap` | integer | Free heap memory in bytes |
| `chipTemperature` | float | Internal chip temperature in °C |
| `sdkVersion` | string | ESP-IDF SDK version |
| `ipAddress` | string | Gateway IP address |
| `macAddress` | string | Gateway WiFi MAC address |
| `rssi` | integer | WiFi signal strength in dBm |
| `timeSynced` | boolean | NTP time sync status |
| `timestamp` | integer | Current Unix timestamp (if time synced) |
| `otaPartition` | string | Active OTA partition label |
| `otaPartitionSize` | integer | OTA partition size in bytes |

### MQTT Topics

The gateway uses these ThingsBoard topics:
- `v1/devices/me/attributes` - Gateway attributes and OTA status
- `v1/devices/me/rpc/request/+` - RPC commands (OTA, reboot, etc.)
- `v1/gateway/connect` - Connect BLE devices
- `v1/gateway/attributes` - BLE device attributes
- `v1/gateway/telemetry` - BLE device telemetry

## Behavior & Operation

### Startup Sequence

1. **Load Configuration** - Read settings from EEPROM (decrypted)
2. **WiFi Connection** - Try to connect to saved WiFi
   - If fails: Enter AP mode for configuration
3. **NTP Time Sync** - Synchronize with NTP servers
   - Continues if sync fails
4. **MQTT Connection** - Connect to ThingsBoard
   - Retries 3 times
   - Falls back to config URL if configured
   - Enters AP mode after 5 minutes of failures
5. **Start Tasks** - Create FreeRTOS tasks
6. **Begin Operation** - Start BLE scanning and MQTT publishing

### Normal Operation

- **BLE Scanning**: Scans every 10 seconds (5-second scan window)
- **Data Processing**: Batches devices every 60 seconds
- **MQTT Publishing**: Sends telemetry batches every 60 seconds
- **MQTT Maintenance**: Keepalive and reconnection checks every 100ms
- **Gateway Status**: Sent every 5 minutes

### Automatic Recovery

- **WiFi Lost**: Attempts reconnection, enters AP mode if fails
- **MQTT Disconnected**: Auto-reconnect every 30 seconds
- **Config Fallback**: Fetches new credentials if MQTT fails
- **Task Watchdog**: System resets and auto-recovers on crash

## Monitoring & Debugging

### Serial Monitor

Baud rate: **115200**

Expected startup output:
```
========================================
BLE Gateway Starting...
Firmware: BLE-Gateway v1.1.0
========================================

Config loaded, trying WiFi and MQTT...
............
WiFi connected!
==== WiFi Status ====
SSID: YourWiFi
IP Address: 192.168.1.100
Signal strength (RSSI): -45 dBm
WiFi Status: Connected
=====================

Synchronizing time with NTP server...
Time synchronized successfully!
Current time: Sun Oct 20 12:34:56 2025

Connecting to MQTT...connected!
Subscribed to:
  - v1/devices/me/attributes
  - v1/devices/me/attributes/response/+
  - v1/devices/me/rpc/request/+

========== Starting FreeRTOS Tasks ==========
All tasks created successfully!

MQTT Maintenance Task started
BLE Scan Task started
Message Processing Task started

WiFi & MQTT OK, starting BLE scan...
Setup complete.

Scanning BLE...
```

### Health Indicators

Monitor these metrics in ThingsBoard:
- `freeHeap` > 100KB (healthy)
- `chipTemperature` < 80°C (healthy)
- `rssi` > -70 dBm (good signal)
- `timeSynced` = true (time accurate)
- `fw_state` = IDLE (no update in progress)

## Troubleshooting

### Gateway Not Scanning BLE Devices

- Verify sensors are powered and advertising
- Check ESP32 BLE antenna is connected
- Ensure sensors are within range (~10m)
- Check Serial Monitor for scan messages

### Cannot Connect to Config Portal

- Look for `BLE-Gateway-Setup` WiFi network
- Password is `12345678`
- Try forgetting and reconnecting to the network
- Check if ESP32 is in AP mode (Serial Monitor shows "AP mode")

### MQTT Connection Fails

- Verify ThingsBoard host is correct
- Check device access token is valid
- Ensure device exists in ThingsBoard
- Check firewall allows outbound port 1883
- Try config URL fallback if configured

### OTA Update Fails

- Ensure firmware URL is accessible from ESP32
- Check firmware file is valid `.bin` format
- Verify firmware size fits in OTA partition (~1.4MB max)
- Ensure WiFi connection is stable
- Check ESP32 has sufficient free heap memory

### Sensors Not Appearing in ThingsBoard

- Check BLE advertisements are received (Serial Monitor)
- Verify MQTT connection is active
- Check ThingsBoard gateway device has proper permissions
- Look for errors in ThingsBoard logs

### Time Not Syncing

- Verify UDP port 123 is not blocked
- Try alternative NTP servers (edit code)
- Check network has internet access
- System continues to operate without time sync

## Current Version

**v1.1.0** - Multi-threading and improved stability (2025-01-19)

### Features in v1.1.0

- ✅ Multi-threaded architecture using FreeRTOS tasks
- ✅ MQTT maintenance task for keepalives and reconnection
- ✅ BLE scanning task for continuous device discovery
- ✅ Message processing task for batch operations
- ✅ NTP time synchronization for accurate timestamps
- ✅ Config URL fallback - fetch MQTT settings from remote server
- ✅ Encrypted credential storage - passwords encrypted in EEPROM
- ✅ Enhanced MQTT stability - improved reconnection logic
- ✅ Gateway temperature reporting - chip temperature in telemetry
- ✅ Thread-safe operations - mutex protection for shared resources

## Contributing

This is a private repository. Contact the author for collaboration opportunities.

## Author

**Matt Burton** ([@mattburt36](https://github.com/mattburt36))

## License

**Private - All Rights Reserved**

This software is proprietary and confidential. Unauthorized copying, distribution, or use is strictly prohibited.

---

**Repository:** https://github.com/mattburt36/BLE-Gateway  
**Version:** v1.1.0  
**Last Updated:** 2025-10-20
