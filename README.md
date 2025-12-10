# BLE Gateway for ThingsBoard

ESP32-based BLE to MQTT gateway that discovers all BLE devices and parses temperature/humidity data from LOP001 sensors. Features multi-threaded operation, intelligent change detection, and seamless ThingsBoard integration.

## Features

- 🔍 **Universal BLE Scanning** - Detects and reports all BLE devices in range
- 🌡️ **LOP001 Sensor Parsing** - Automatic temperature/humidity extraction from LOP001 beacons
- 📡 **ThingsBoard Integration** - Direct MQTT publishing to ThingsBoard with device auto-provisioning
- 🧵 **Multi-threaded** - FreeRTOS tasks for stable concurrent operation
- 🔄 **Smart Change Detection** - Only publishes when data changes significantly
- ⏰ **6-Hour Keepalive** - Periodic updates even when values are stable
- 📶 **WiFi Portal** - Web-based configuration interface
- 🔐 **Secure Storage** - Encrypted credential storage in flash
- ⏱️ **NTP Time Sync** - Accurate timestamps on all telemetry
- 🚀 **OTA Updates** - Remote firmware updates via ThingsBoard or HTTP/HTTPS

## Hardware Requirements

- **ESP32 Development Board** (tested on XIAO ESP32-S3)
- **LOP001 BLE Sensors** - optional, gateway reports all BLE devices

## Supported Sensors

### LOP001 Temperature Beacon
- **Device Name:** `LOP001`
- **Service UUID:** `0x181A` (Environmental Sensing Service)
- **Sensor:** SHT40 Temperature & Humidity
- **Data:** Temperature (°C), Humidity (%)
- **Resolution:** 0.01°C / 0.01%RH
- **Range:** -40°C to 125°C, 0-100% RH
- **Data Format:** 
  - Bytes 0-1: Temperature (sint16, little-endian)
  - Bytes 2-3: Humidity (uint16, little-endian)

### All Other BLE Devices
- **Reported as:** `BLE_DEVICE`
- **Data:** MAC address, name, RSSI only (no temperature/humidity)

## Quick Start

### 1. Flash the Gateway

#### Option A: PlatformIO (Recommended)

```bash
# Clone the repository
git clone https://github.com/Hoptech/BLE-Gateway
cd BLE-Gateway

# Open in VS Code with PlatformIO
code .

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
```

#### Option B: Arduino IDE

1. Install Arduino IDE with ESP32 board support
2. Install required libraries:
   - PubSubClient
   - ArduinoJson (v7.x)
   - ESP32 BLE Arduino
3. Open `src/main.cpp`
4. Select board: **ESP32 Dev Module** (or **XIAO ESP32-S3**)
5. Upload

### 2. Configure WiFi and MQTT

On first boot, the gateway creates a WiFi access point:

1. Connect to WiFi network: **BLE-Gateway-Setup**
2. Password: `12345678`
3. Navigate to: `http://192.168.4.1`
4. Enter your settings:
   - **WiFi SSID:** Your WiFi network name
   - **WiFi Password:** Your WiFi password
   - **MQTT Host:** `mqtt.hoptech.co.nz` (or your broker)
   - **MQTT Username:** `thingsboard`
   - **MQTT Password:** Your broker password
5. Click **Save and Restart**

The gateway will connect and begin scanning for BLE devices immediately.

### 3. Configure ThingsBoard MQTT Connector

In ThingsBoard, create an MQTT connector with this configuration:

```json
{
  "broker": {
    "host": "mqtt.hoptech.co.nz",
    "port": 1883,
    "version": 5,
    "clientId": "Thingsboard",
    "security": {
      "type": "basic",
      "username": "thingsboard",
      "password": "your-password"
    }
  },
  "mapping": [
    {
      "topicFilter": "sensor/data",
      "subscriptionQos": 1,
      "converter": {
        "type": "json",
        "deviceInfo": {
          "deviceNameExpressionSource": "message",
          "deviceNameExpression": "${serialNumber}",
          "deviceProfileExpressionSource": "message",
          "deviceProfileExpression": "${sensorType}"
        },
        "sendDataOnlyOnChange": false,
        "timeout": 60000,
        "useServerSideTimestamp": false,
        "timestampExpressionSource": "message",
        "timestampExpression": "${timestamp}",
        "timestampFormat": "SECONDS",
        "attributes": [
          {
            "type": "string",
            "key": "model",
            "value": "${sensorModel}"
          }
        ],
        "timeseries": [
          {
            "type": "double",
            "key": "temperature",
            "value": "${temp}"
          },
          {
            "type": "double",
            "key": "humidity",
            "value": "${hum}"
          },
          {
            "type": "integer",
            "key": "rssi",
            "value": "${rssi}"
          },
          {
            "type": "integer",
            "key": "battery",
            "value": "${battery}"
          }
        ]
      }
    }
  ],
  "requestsMapping": {
    "connectRequests": [
      {
        "topicFilter": "sensor/connect",
        "deviceInfo": {
          "deviceNameExpressionSource": "message",
          "deviceNameExpression": "${serialNumber}",
          "deviceProfileExpressionSource": "constant",
          "deviceProfileExpression": "BLE-Gateway"
        }
      }
    ],
    "disconnectRequests": [
      {
        "topicFilter": "sensor/disconnect",
        "deviceInfo": {
          "deviceNameExpressionSource": "message",
          "deviceNameExpression": "${serialNumber}"
        }
      }
    ],
    "attributeUpdates": [
      {
        "retain": true,
        "deviceNameFilter": ".*",
        "attributeFilter": "firmwareVersion",
        "topicExpression": "sensor/${deviceName}/firmwareVersion",
        "valueExpression": "{\"firmwareVersion\":\"${attributeValue}\"}"
      }
    ],
    "serverSideRpc": [
      {
        "type": "twoWay",
        "deviceNameFilter": ".*",
        "methodFilter": "echo",
        "requestTopicExpression": "sensor/${deviceName}/request/${methodName}/${requestId}",
        "responseTopicExpression": "sensor/${deviceName}/response/${methodName}/${requestId}",
        "responseTopicQoS": 1,
        "responseTimeout": 10000,
        "valueExpression": "${params}"
      }
    ]
  }
}
```

## Over-The-Air (OTA) Firmware Updates

The gateway supports remote firmware updates via ThingsBoard or direct MQTT commands.

### Method 1: ThingsBoard Shared Attributes (Recommended)

1. In ThingsBoard, navigate to your gateway device
2. Go to **Attributes** → **Shared attributes**
3. Add or update the `firmwareVersion` attribute with the firmware URL:
   ```
   http://your-server.com/firmware/ble-gateway-v2.1.0.bin
   ```
   or
   ```
   https://your-server.com/firmware/ble-gateway-v2.1.0.bin
   ```

The gateway will automatically:
- Receive the attribute update via MQTT
- Download the firmware (supports both HTTP and HTTPS)
- Verify the download
- Apply the update
- Reboot with new firmware

### Method 2: Direct MQTT Command

Publish to topic: `gateway/{DEVICE_ID}/ota`

Payload:
```json
{
  "version": "2.1.0",
  "url": "http://your-server.com/firmware/ble-gateway-v2.1.0.bin",
  "size": 1234567
}
```

### Method 3: RPC Command (future)

Execute RPC method `updateFirmware` with firmware URL as parameter.

### OTA Status Monitoring

The gateway publishes OTA status to: `gateway/{DEVICE_ID}/ota/status`

Status messages:
```json
{
  "device_id": "A1B2C3D4E5F6",
  "status": "downloading",
  "progress": 45,
  "current_version": "2.0.0"
}
```

Possible status values:
- `downloading` - Firmware download in progress
- `updating` - Writing firmware to flash
- `success` - Update completed, device will reboot
- `failed` - Update failed (see error field)
- `up_to_date` - Already running requested version

### Preparing Firmware Files

1. Build your firmware with PlatformIO:
   ```bash
   pio run
   ```

2. The firmware binary is located at:
   ```
   .pio/build/seeed_xiao_esp32s3/firmware.bin
   ```

3. Upload to your web server or file hosting service

4. Ensure the URL is accessible from your gateway devices

### Security Considerations

- **HTTP**: Fast but unencrypted (use for local/trusted networks)
- **HTTPS**: Encrypted but currently uses insecure mode (no certificate validation)
- For production, consider hosting firmware on your own server with proper SSL certificates
- The gateway verifies firmware size and integrity during download

### Troubleshooting OTA

**Update fails with "HTTP error":**
- Check firmware URL is accessible
- Verify server is responding
- Check firewall rules

**Update fails with "Not enough space":**
- Firmware may be too large for partition
- Check `platformio.ini` partition scheme
- Current scheme: `huge_app.csv`

**Update fails with "Incomplete download":**
- Network connectivity issues
- Server timeout
- Try again with stable connection

**Device doesn't reboot after update:**
- Check serial output for errors
- Manually power cycle the device
- Previous firmware may still be running

## Architecture

### Multi-Threaded Design

The gateway uses FreeRTOS tasks for reliable concurrent operation:

#### Task 1: BLE Scanner (Core 1, Priority 1)
- Continuously scans for BLE advertisements
- Scans every 10 seconds (5-second scan window)
- Parses LOP001 sensor data automatically
- Reports all BLE devices regardless of type
- Runs on dedicated core for optimal performance

#### Task 2: MQTT Maintenance (Core 0, Priority 2)
- Maintains MQTT connection with automatic reconnection
- Processes incoming messages and RPC commands
- Sends periodic gateway status updates (every 5 minutes)
- Handles connection failures gracefully

#### Task 3: WiFi Monitor (Core 0, Priority 1)
- Monitors WiFi connection status
- Automatic reconnection on WiFi loss
- Falls back to AP mode if WiFi fails repeatedly

#### Task 4: Device Tracker (Core 0, Priority 1)
- Tracks all discovered BLE devices
- Smart change detection (only publishes when values change)
- 6-hour keepalive for stable sensors
- Automatic expiry of devices not seen for 6 hours

### Data Flow

```
BLE Devices → BLE Scanner → Device Tracker → MQTT Publisher → ThingsBoard
               (parse LOP001) (change detect)   (batch send)    (auto-provision)
```

## MQTT Topics and Messages

### Published by Gateway

#### Device Connect
**Topic:** `sensor/connect`

```json
{
  "serialNumber": "A1B2C3D4E5F6",
  "sensorType": "BLE-Gateway",
  "sensorModel": "XIAO-ESP32-S3",
  "firmware": "2.0.0"
}
```

#### Device Data (LOP001 Sensors)
**Topic:** `sensor/data`

```json
{
  "serialNumber": "A1B2C3D4E5F6",
  "sensorType": "LOP001",
  "sensorModel": "LOP001",
  "temp": "26.11",
  "hum": "56.90",
  "battery": 0,
  "rssi": -65,
  "gateway": "GATEWAY_MAC",
  "timestamp": 1700000000
}
```

#### Device Data (Non-LOP001 BLE Devices)
**Topic:** `sensor/data`

```json
{
  "serialNumber": "AA:BB:CC:DD:EE:FF",
  "sensorType": "BLE_DEVICE",
  "sensorModel": "BLE_DEVICE",
  "temp": "",
  "hum": "",
  "rssi": -72,
  "gateway": "GATEWAY_MAC",
  "timestamp": 1700000000
}
```

#### Gateway Status
**Topic:** `sensor/data`

```json
{
  "serialNumber": "GATEWAY_MAC",
  "sensorType": "BLE-Gateway",
  "sensorModel": "XIAO-ESP32-S3",
  "temp": 0,
  "hum": 0,
  "firmware": "2.0.0",
  "uptime": 3600,
  "freeHeap": 180000,
  "wifiRssi": -45,
  "timestamp": 1700000000
}
```

#### Device Disconnect
**Topic:** `sensor/disconnect`

```json
{
  "serialNumber": "A1B2C3D4E5F6"
}
```

## Configuration

### Web Portal Settings

Access at `http://192.168.4.1` when in AP mode:

- **WiFi SSID:** Your WiFi network name
- **WiFi Password:** WiFi password (encrypted before storage)
- **MQTT Host:** ThingsBoard MQTT broker (e.g., `mqtt.hoptech.co.nz`)
- **MQTT Username:** MQTT authentication username
- **MQTT Password:** MQTT authentication password (encrypted before storage)

All credentials are encrypted before being stored in flash memory.

### Adjustable Parameters

Edit these in the code to customize behavior:

```cpp
// BLE scanning (ble_scanner.h)
const int SCAN_TIME = 5;        // Scan duration in seconds
const int SCAN_INTERVAL = 10;   // Seconds between scans

// Change detection thresholds (device_tracker.h)
const float TEMP_THRESHOLD = 0.5;     // °C
const float HUM_THRESHOLD = 2.0;      // %
const int BATTERY_THRESHOLD = 5;      // % or mV

// Keepalive interval (device_tracker.h)
const unsigned long SIX_HOURS = 6 * 60 * 60 * 1000;  // milliseconds

// Status reporting (mqtt_handler.h)
const unsigned long STATUS_INTERVAL = 300000;  // 5 minutes

// MQTT settings (mqtt_handler.h)
const int MQTT_PORT = 1883;
const int MQTT_KEEPALIVE_SEC = 60;

// Firmware version (main.cpp)
#define FIRMWARE_VERSION "2.0.0"
#define FIRMWARE_TITLE "BLE-Gateway-XIAO"
```

## Behavior & Operation

### Startup Sequence

1. **Initialize Hardware** - LED blinks 3 times
2. **Load Configuration** - Read encrypted settings from flash
3. **WiFi Connection** - Attempts to connect to saved WiFi
   - On failure: Enters AP mode for configuration
4. **NTP Time Sync** - Synchronizes with NTP servers
   - Continues if sync fails
5. **MQTT Connection** - Connects to ThingsBoard broker
   - Retries every 10 seconds on failure
6. **Start Tasks** - Creates all FreeRTOS tasks
7. **Begin Scanning** - Starts BLE device discovery

### LED Status Indicators

- **3 quick blinks at startup** - Gateway booting
- **Slow blink** - AP mode (configuration portal active)
- **2 quick blinks** - WiFi connected
- **Solid ON** - MQTT connected and operational

### Normal Operation

- **BLE Scanning:** Every 10 seconds (5-second scan window)
- **Change Detection:** Only publishes when data changes significantly
- **Keepalive:** Publishes every 6 hours even if data is stable
- **Gateway Status:** Published every 5 minutes
- **Device Expiry:** Devices not seen for 6 hours are removed from tracking

### Smart Change Detection

The gateway only publishes telemetry when values change significantly:

- **Temperature:** ≥ 0.5°C change
- **Humidity:** ≥ 2.0% change
- **Battery:** ≥ 5% (or 5mV) change

This reduces unnecessary MQTT traffic and ThingsBoard storage.

## ThingsBoard Device Types

Devices are automatically categorized in ThingsBoard:

| Device Type | Description | Temperature | Humidity | Battery |
|------------|-------------|-------------|----------|---------|
| `LOP001` | LOP001 Temperature Beacon | ✅ | ✅ | ❌ |
| `BLE_DEVICE` | Unknown BLE device | ❌ | ❌ | ❌ |
| `BLE-Gateway` | Gateway itself | ❌ | ❌ | ❌ |

## Monitoring & Debugging

### Serial Monitor

Baud rate: **115200**

Expected startup output:

```
========================================
BLE-Gateway-XIAO v2.0.0
XIAO ESP32-S3 BLE Gateway
========================================
Device ID: A1B2C3D4E5F6

Configuration loaded from flash.
WiFi SSID: MyNetwork
MQTT Host: mqtt.hoptech.co.nz
MQTT User: thingsboard

WiFi connected!
Time synchronized via NTP
Current time: Mon Nov 17 12:00:00 2025

MQTT connected successfully

Creating FreeRTOS tasks...
Initializing BLE scanner...
✓ BLE scanner initialized (duplicates enabled)
All tasks created successfully!

BLE Scan Task started
MQTT Maintenance Task started
WiFi Monitor Task started
Device Tracker Task started

Setup complete.

Starting BLE scan...
New device discovered: A1:B2:C3:D4:E5:F6 (LOP001)
  Type: LOP001, Temp: 26.11°C, Humidity: 56.90%, Battery: 0, RSSI: -65

New device discovered: AA:BB:CC:DD:EE:FF (Unknown)
  Type: BLE_DEVICE, RSSI: -72

Published device: A1:B2:C3:D4:E5:F6
📤 Published to sensor/data (size: 156 bytes)
   Device: A1:B2:C3:D4:E5:F6, Temp: 26.11°C, Hum: 56.90%

Published device: AA:BB:CC:DD:EE:FF
📤 Published to sensor/data (size: 142 bytes)
   Device: AA:BB:CC:DD:EE:FF (non-sensor, RSSI: -72)
```

### Health Indicators

Monitor these metrics in ThingsBoard:

- **freeHeap:** > 100KB (healthy)
- **wifiRssi:** > -70 dBm (good signal)
- **uptime:** Increasing steadily (no crashes)
- **Device count:** Matches expected BLE devices in range

## Troubleshooting

### Gateway Not Scanning BLE Devices

- Verify ESP32 BLE is initialized (check serial output)
- Ensure BLE devices are powered and advertising
- Check devices are within range (~10m)
- Look for "BLE Scan Task started" in serial output

### Cannot Connect to Config Portal

- Look for `BLE-Gateway-Setup` WiFi network
- Password is `12345678`
- Try forgetting and reconnecting
- Check serial output for "AP mode" message

### MQTT Connection Fails

- Verify broker host is correct (`mqtt.hoptech.co.nz`)
- Check username/password are correct
- Ensure broker is accessible (ping the host)
- Check firewall allows outbound port 1883
- Review detailed MQTT connection logs in serial output

### LOP001 Sensors Not Parsing

- Verify sensor is advertising (use BLE scanner app)
- Check device name is exactly `LOP001`
- Check service UUID is `0x181A` (Environmental Sensing Service)
- Ensure sensor data is within valid ranges:
  - Temperature: -40°C to 125°C
  - Humidity: 0% to 100%

### Devices Not Appearing in ThingsBoard

- Verify MQTT connector is configured correctly
- Check topic filter is `sensor/data`
- Ensure connector is active
- Review ThingsBoard event logs for errors
- Verify gateway is publishing (check serial output)

### Time Not Syncing

- Verify UDP port 123 is not blocked
- Check network has internet access
- System continues to operate without NTP sync
- Timestamps will be relative to uptime

## Project Structure

```
BLE-Gateway/
├── src/
│   ├── main.cpp              # Main application and setup
│   ├── ble_scanner.h         # BLE scanning and LOP001 parsing
│   ├── config_manager.h      # Configuration storage and encryption
│   ├── device_tracker.h      # Device tracking and change detection
│   ├── mqtt_handler.h        # MQTT connection and publishing
│   ├── ota_manager.h         # OTA firmware updates
│   └── wifi_manager.h        # WiFi and configuration portal
├── platformio.ini            # PlatformIO configuration
└── README.md                 # This file
```

## Version History

### v2.0.0 (2025-11-17)
- ✅ Universal BLE device reporting
- ✅ LOP001-specific temperature/humidity parsing
- ✅ Direct ThingsBoard MQTT connector integration
- ✅ Smart change detection with configurable thresholds
- ✅ 6-hour keepalive for stable sensors
- ✅ Multi-threaded FreeRTOS architecture
- ✅ Encrypted credential storage
- ✅ NTP time synchronization
- ✅ Web-based configuration portal
- ✅ Automatic WiFi and MQTT reconnection
- ✅ OTA firmware updates via ThingsBoard attributes
- ✅ HTTP and HTTPS firmware download support
- ✅ OTA progress reporting and error handling

## License

Private - All Rights Reserved

This software is proprietary and confidential. Unauthorized copying, distribution, or use is strictly prohibited.

## Author

**Hoptech Limited**  
Gateway developed for ThingsBoard sensor monitoring

---

**Repository:** https://github.com/Hoptech/BLE-Gateway  
**Version:** v2.0.0  
**Last Updated:** 2025-11-17

## Device Provisioning

### Quick Start

Each BLE Gateway device needs to be provisioned with unique MQTT credentials before it can connect to the broker.

```bash
# Provision a new device
./provision_device.sh
```

For complete provisioning documentation, see:
- **[PROVISIONING_QUICKREF.md](PROVISIONING_QUICKREF.md)** - Quick reference
- **[PROVISIONING.md](PROVISIONING.md)** - Complete guide
- **[PROVISIONING_CHANGES.md](PROVISIONING_CHANGES.md)** - Implementation details

### Security

- ✅ **No default credentials** - Each device requires explicit provisioning
- ✅ **Encrypted storage** - Credentials stored in encrypted NVS flash
- ✅ **Unique credentials** - Each device gets unique username/password
- ✅ **QoS 1** - MQTT subscriptions use QoS 1 for reliable delivery

