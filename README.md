cat > README.md << 'EOF'
# BLE Gateway for ThingsBoard

ESP32-based BLE to MQTT gateway for MOKO L02S temperature/humidity sensors with OTA firmware update support.

## Features

- BLE scanning and advertisement parsing
- MQTT gateway integration with ThingsBoard
-Temperature & Humidity sensor support (MOKO L02S, MOKO T&H)
- OTA firmware updates via ThingsBoard
- WiFi configuration portal
- Smart deduplication and batch sending
- Automatic device profile assignment
- Battery monitoring

## Hardware Requirements

- ESP32 DevKit (or compatible)
- MOKO L02S or compatible BLE sensors

## Supported Sensors

- **Hoptech/MOKO L02S-EA01** (Service UUID: 0000ea01)
  - Temperature (0.1°C resolution)
  - Humidity (0.1% resolution)
  - Battery voltage

- **MOKO T&H Series** (Service UUID: 0xABFE)
  - Temperature (0.1°C resolution)
  - Humidity (0.1% resolution)
  - Battery voltage
  - Device type detection

## Installation

1. Open in Arduino IDE
2. Install required libraries (see Dependencies)
3. Upload to ESP32
4. Connect to `BLE-Gateway-Setup` WiFi (password: `12345678`)
5. Configure WiFi and ThingsBoard credentials
6. Gateway will auto-connect and start scanning

## Configuration

On first boot or when config is missing:
- Gateway creates WiFi AP: `BLE-Gateway-Setup`
- Connect and navigate to `192.168.4.1`
- Enter:
  - WiFi SSID
  - WiFi Password
  - ThingsBoard MQTT Host (e.g., `mqtt.thingsboard.cloud`)
  - Device Access Token

## OTA Updates

The gateway supports remote firmware updates via ThingsBoard:

### Via ThingsBoard Shared Attributes:
```json
{
  "fw_title": "BLE-Gateway",
  "fw_version": "1.1.0",
  "fw_url": "https://your-server.com/firmware.bin",
  "fw_size": 920000
}

Via RPC:
```json
{
  "method": "updateFirmware",
  "params": {
    "fw_title": "BLE-Gateway",
    "fw_version": "1.1.0",
    "fw_url": "https://your-server.com/firmware.bin"
  }
}
The gateway reports:

    current_fw_version: Currently running version
    fw_state: Update status (IDLE/DOWNLOADING/UPDATING/UPDATED/FAILED)
    fw_progress: Progress percentage (0-100)

Other RPC Commands:

    getCurrentFirmware - Get current firmware info
    reboot - Restart the gateway

Current Firmware Version

v1.0.0 - Initial release (2025-01-17)
Dependencies

    WiFi (ESP32 built-in)
    PubSubClient
    ArduinoJson (v7.x)
    ESP32 BLE Arduino
    HTTPClient (for OTA)
    Update (ESP32 OTA)

ThingsBoard Integration
Device Types Created:

    L02S - Hoptech/MOKO L02S sensors
    MOKO_TH - MOKO T&H sensors
    MOKO_3AXIS - MOKO 3-axis sensors
    MOKO_3AXIS_TH - MOKO combined sensors
    BLE_SENSOR - Generic BLE sensors
    BLE_BEACON - Unknown BLE devices

Telemetry Sent:

    temperature (°C)
    humidity (%)
    battery_mv (millivolts)
    rssi (dBm)
    name (device name)
    manufacturerData (hex string)
    serviceData (hex string)

Attributes Sent:

    macAddress
    deviceName
    sensorType
    hasTemperature
    hasHumidity
    hasBattery
    current_fw_version
    chipModel
    ipAddress
    freeHeap

Architecture
Code

BLE Sensors → ESP32 Gateway → MQTT → ThingsBoard
                    ↓
              OTA Updates (HTTP)

Author

Matt Burton (@mattburt36)

License
Private - All Rights Reserved

Changelog

v1.0.0 (2025-01-17)
    Initial release
    BLE scanning with MOKO L02S support
    MQTT gateway integration
    OTA firmware updates
    WiFi configuration portal
    Smart deduplication
    Device profile assignment
