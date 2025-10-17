# BLE Gateway for ThingsBoard

ESP32-based BLE to MQTT gateway for MOKO L02S temperature/humidity sensors with OTA firmware update support.

## Features

- 🔍 BLE scanning and advertisement parsing
- 📡 MQTT gateway integration with ThingsBoard
- 🌡️ Temperature & Humidity sensor support (MOKO L02S, MOKO T&H)
- 🔄 OTA firmware updates via ThingsBoard
- 📶 WiFi configuration portal
- 🎯 Smart deduplication and batch sending
- 🏷️ Automatic device profile assignment
- 🔋 Battery monitoring

## Hardware Requirements

- ESP32 DevKit (or compatible)
- MOKO L02S or compatible BLE sensors

## Supported Sensors

### Hoptech/MOKO L02S-EA01
- **Service UUID:** `0000ea01-0000-1000-8000-00805f9b34fb`
- Temperature (0.1°C resolution)
- Humidity (0.1% resolution)
- Battery voltage (mV)

### MOKO T&H Series
- **Service UUID:** `0xABFE` (frame type: `0x70`)
- Temperature (0.1°C resolution)
- Humidity (0.1% resolution)
- Battery voltage (mV)
- Device type detection (3-axis, T&H, combined)

## Installation

1. Open `BLE-WiFi-Gateway.ino` in Arduino IDE
2. Install required libraries (see Dependencies)
3. Upload to ESP32
4. Connect to `BLE-Gateway-Setup` WiFi network (password: `12345678`)
5. Navigate to `192.168.4.1` and configure WiFi and ThingsBoard credentials
6. Gateway will auto-connect and start scanning

## Configuration

On first boot or when config is missing:

1. Gateway creates WiFi AP: **`BLE-Gateway-Setup`**
2. Connect to it (password: `12345678`)
3. Navigate to `192.168.4.1` in your browser
4. Enter:
   - **WiFi SSID:** Your WiFi network name
   - **WiFi Password:** Your WiFi password
   - **ThingsBoard MQTT Host:** e.g., `mqtt.thingsboard.cloud`
   - **Device Access Token:** Your gateway device token from ThingsBoard
5. Click "Save and Restart"

The configuration is stored in EEPROM and persists across reboots.

## OTA Firmware Updates

The gateway supports remote firmware updates via ThingsBoard.

### Method 1: Shared Attributes

Set these shared attributes on your gateway device in ThingsBoard:

```json
{
  "fw_title": "BLE-Gateway",
  "fw_version": "1.1.0",
  "fw_url": "https://your-server.com/firmware.bin",
  "fw_size": 920000,
  "fw_checksum": "sha256_hash_optional"
}
```

### Method 2: RPC Call

Send an RPC command to your gateway:

```json
{
  "method": "updateFirmware",
  "params": {
    "fw_title": "BLE-Gateway",
    "fw_version": "1.1.0",
    "fw_url": "https://your-server.com/firmware.bin",
    "fw_size": 920000
  }
}
```

### OTA Status Reporting

The gateway reports its firmware status via device attributes:

- `current_fw_title`: Current firmware name
- `current_fw_version`: Currently running version
- `fw_state`: Update status
  - `IDLE` - No update in progress
  - `DOWNLOADING` - Downloading firmware
  - `UPDATING` - Flashing firmware
  - `UPDATED` - Update complete (will reboot)
  - `FAILED` - Update failed
  - `UP_TO_DATE` - Already on latest version
- `fw_progress`: Progress percentage (0-100)

### Other RPC Commands

```json
// Get current firmware information
{
  "method": "getCurrentFirmware"
}

// Reboot the gateway
{
  "method": "reboot"
}
```

## Current Firmware Version

**v1.0.0** - Initial release (2025-01-17)

## Dependencies

### Required Libraries

Install these via Arduino Library Manager:

- **WiFi** (ESP32 built-in)
- **PubSubClient** (MQTT client)
- **ArduinoJson** (v7.x) - JSON parsing
- **ESP32 BLE Arduino** (BLE functionality)
- **HTTPClient** (ESP32 built-in, for OTA)
- **Update** (ESP32 built-in, for OTA)
- **EEPROM** (ESP32 built-in)
- **WebServer** (ESP32 built-in)
- **DNSServer** (ESP32 built-in)

### Board Support

- **ESP32 Arduino Core** - Install via Boards Manager
  - Board: ESP32 Dev Module (or your specific board)

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

The gateway sends the following telemetry for each device:

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

The gateway sends the following attributes for each device:

| Key | Type | Description |
|-----|------|-------------|
| `macAddress` | string | BLE MAC address |
| `deviceName` | string | BLE advertised name |
| `sensorType` | string | Detected sensor type |
| `hasTemperature` | boolean | Supports temperature readings |
| `hasHumidity` | boolean | Supports humidity readings |
| `hasBattery` | boolean | Reports battery voltage |

### Gateway Attributes

The gateway reports its own telemetry and attributes:

| Key | Type | Description |
|-----|------|-------------|
| `current_fw_title` | string | Firmware name |
| `current_fw_version` | string | Current firmware version |
| `fw_state` | string | OTA update status |
| `chipModel` | string | ESP32 chip model |
| `chipRevision` | integer | ESP32 chip revision |
| `cpuFreqMHz` | integer | CPU frequency in MHz |
| `flashSize` | integer | Flash size in bytes |
| `freeHeap` | integer | Free heap memory in bytes |
| `sdkVersion` | string | ESP-IDF SDK version |
| `ipAddress` | string | Gateway IP address |
| `macAddress` | string | Gateway WiFi MAC address |
| `rssi` | integer | WiFi signal strength in dBm |
| `otaPartition` | string | Active OTA partition label |
| `otaPartitionSize` | integer | OTA partition size in bytes |

## Architecture

```
BLE Sensors → ESP32 Gateway → MQTT → ThingsBoard
                    ↓
              OTA Updates (HTTPS)
```

### Data Flow

1. **BLE Scanning:** Gateway scans for BLE advertisements every 10 seconds
2. **Parsing:** Advertisements are parsed based on service UUID and data format
3. **Deduplication:** Identical advertisements are filtered (only RSSI updated)
4. **Batching:** Detected devices are batched and sent every 60 seconds
5. **MQTT Publishing:**
   - Device connection messages (`v1/gateway/connect`)
   - Device attributes (`v1/gateway/attributes`)
   - Device telemetry (`v1/gateway/telemetry`)
6. **OTA Updates:** Gateway listens for firmware update requests via MQTT

### Topics Used

- `v1/devices/me/attributes` - Gateway attributes and OTA status
- `v1/devices/me/rpc/request/+` - RPC commands (OTA, reboot, etc.)
- `v1/gateway/connect` - Connect BLE devices
- `v1/gateway/attributes` - BLE device attributes
- `v1/gateway/telemetry` - BLE device telemetry

## Configuration Options

### Adjustable Parameters

Edit these in the code to customize behavior:

```cpp
// BLE scanning
const int SCAN_TIME = 5;  // BLE scan duration (seconds)
unsigned long last_ble_scan = 0;
// Scan interval: 10000ms (10 seconds)

// Batch sending
const unsigned long BATCH_INTERVAL = 60000;  // 60 seconds

// MQTT reconnection
const unsigned long MQTT_FAIL_AP_TIMEOUT = 300000;  // 5 minutes

// Firmware version
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_TITLE "BLE-Gateway"

// WiFi AP credentials
WiFi.softAP("BLE-Gateway-Setup", "12345678");
```

## Troubleshooting

### Gateway Not Scanning BLE Devices

- Check ESP32 has BLE antenna connected
- Verify sensors are powered on and advertising
- Check Serial Monitor for BLE scan messages
- Ensure sensors are within range (~10m)

### Cannot Connect to Config Portal

- Disconnect from other WiFi networks
- Look for `BLE-Gateway-Setup` WiFi network
- Try forgetting the network and reconnecting
- Check if ESP32 LED is blinking (indicates AP mode)

### MQTT Connection Fails

- Verify ThingsBoard host is correct
- Check device access token is valid
- Ensure ThingsBoard device exists and is not deleted
- Check firewall allows outbound port 1883
- Verify WiFi has internet access

### OTA Update Fails

- Ensure firmware URL is accessible from ESP32
- Check firmware file is valid `.bin` format
- Verify firmware size fits in OTA partition (~1.4MB max)
- Check ESP32 has sufficient free heap memory
- Ensure WiFi connection is stable during update

### Sensors Not Appearing in ThingsBoard

- Check BLE advertisements are being received (Serial Monitor)
- Verify MQTT connection is active
- Check ThingsBoard gateway device has proper permissions
- Look for errors in ThingsBoard logs
- Ensure device types exist in ThingsBoard

## Development

### Build Instructions

1. Clone repository: `git clone git@github.com:mattburt36/BLE-Gateway.git`
2. Open `BLE-WiFi-Gateway.ino` in Arduino IDE
3. Select board: **ESP32 Dev Module**
4. Configure board settings:
   - Upload Speed: 921600
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB (32Mb)
   - Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
5. Select port and upload

### Export Compiled Binary (for OTA)

1. In Arduino IDE: Sketch → Export compiled Binary
2. Binary will be saved in sketch folder as `BLE-WiFi-Gateway.ino.esp32.bin`
3. Upload this file to your web server for OTA updates

### Serial Monitor Output

Baud rate: **115200**

Expected output:
```
========================================
BLE Gateway Starting...
Firmware: BLE-Gateway v1.0.0
========================================

Config loaded, trying WiFi and MQTT...
..........
WiFi connected!
==== WiFi Status ====
SSID: YourWiFi
IP Address: 192.168.1.100
Signal strength (RSSI): -45 dBm
WiFi Status: Connected
=====================
Connecting to MQTT...connected!
Subscribed to:
  - v1/devices/me/attributes
  - v1/devices/me/attributes/response/+
  - v1/devices/me/rpc/request/+
WiFi & MQTT OK, starting BLE scan...
Setup complete.
Scanning BLE...

========== BLE Advertisement Detected ==========
MAC Address: f0:74:bf:f0:63:5a
RSSI: -60 dBm
Name: Hoptech Sensor
...
```

## Security Considerations

### Credentials Storage

- WiFi and MQTT credentials are stored in EEPROM
- Consider encrypting EEPROM data for production deployments
- Use strong WiFi passwords
- Rotate ThingsBoard access tokens periodically

### Network Security

- Use MQTT over TLS (port 8883) for production
- Consider VPN for remote gateway deployments
- Implement firewall rules to restrict access
- Use private WiFi networks when possible

### OTA Security

- **Always use HTTPS URLs** for firmware downloads
- Verify firmware checksums before flashing
- Implement rollback mechanisms for failed updates
- Test firmware thoroughly before deployment

## Future Enhancements

- [ ] MQTT over TLS/SSL (port 8883)
- [ ] Encrypted EEPROM storage
- [ ] Web dashboard for configuration
- [ ] Support for more BLE sensor types
- [ ] Local data caching during network outages
- [ ] Bluetooth mesh support
- [ ] Multiple ThingsBoard server support
- [ ] Configurable scan intervals via ThingsBoard
- [ ] Firmware rollback on failed updates
- [ ] Battery-powered operation with deep sleep

## Contributing

This is a private repository. Contact the author for collaboration opportunities.

## Author

**Matt Burton** ([@mattburt36](https://github.com/mattburt36))

## License

**Private - All Rights Reserved**

This software is proprietary and confidential. Unauthorized copying, distribution, or use is strictly prohibited.

## Changelog

### v1.0.0 (2025-01-17)

**Initial Release**

- ✅ BLE scanning with MOKO L02S support
- ✅ MOKO T&H series support
- ✅ MQTT gateway integration with ThingsBoard
- ✅ OTA firmware updates via ThingsBoard
- ✅ WiFi configuration portal
- ✅ Smart deduplication
- ✅ Automatic device profile assignment
- ✅ Device connect/disconnect handling
- ✅ Battery monitoring
- ✅ Gateway telemetry and diagnostics
- ✅ RPC command support (updateFirmware, getCurrentFirmware, reboot)

---

**Repository:** https://github.com/mattburt36/BLE-Gateway  
**Documentation Version:** 1.0.0  
**Last Updated:** 2025-01-17
