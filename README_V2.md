# BLE Gateway v2.0.0 - Fresh Start

## Project Structure

```
BLE-Gateway/
├── platformio.ini          # PlatformIO configuration for XIAO ESP32-S3
├── src/
│   ├── main.cpp           # Main application and setup
│   ├── config_manager.h   # Flash storage (Preferences API)
│   ├── wifi_manager.h     # WiFi, AP mode, web portal, NTP, remote config
│   ├── mqtt_handler.h     # MQTT connection and publishing
│   ├── ble_scanner.h      # BLE scanning and sensor parsing
│   └── device_tracker.h   # 12-hour change detection logic
└── README_V2.md           # This file
```

## Hardware

**Target Platform:** Seeed XIAO ESP32-S3
- Dual-core ESP32-S3
- Built-in WiFi and BLE
- Compact form factor

## Architecture Overview

### Startup Sequence

1. **Flash Check** → If empty, enter AP mode for configuration
2. **WiFi Connection** → Connect using stored credentials
3. **NTP Sync** → Get timestamp from pool.ntp.org (proof of internet)
4. **Remote Config** → Fetch config from `gwconfig.hoptech.co.nz/{device_id}`
5. **MQTT Connection** → Connect to mqtt.hoptech.co.nz (or configured host)
6. **Start Tasks** → Launch 4 FreeRTOS tasks

### FreeRTOS Tasks

#### Task 1: BLE Scanning (Core 1, Priority 1)
- **Purpose:** Continuous BLE device discovery
- **Scan Interval:** Every 10 seconds (5-second scan window)
- **Supported Sensors:**
  - MOKO L02S-EA01 (Service UUID: 0xEA01)
  - MOKO T&H Series (Service UUID: 0xABFE, frame 0x70)
- **Updates:** Calls `updateDevice()` for each discovered sensor

#### Task 2: MQTT Maintenance (Core 0, Priority 2)
- **Purpose:** Maintain MQTT connection and process messages
- **Runs:** Every 100ms
- **Functions:**
  - Auto-reconnect if connection lost
  - Process incoming MQTT messages
  - Send gateway status every 5 minutes
  - Thread-safe MQTT operations (mutex protected)

#### Task 3: WiFi Monitor (Core 0, Priority 1)
- **Purpose:** Ensure WiFi stays connected
- **Runs:** Every 10 seconds
- **Functions:**
  - Detect WiFi disconnections
  - Attempt automatic reconnection
  - Update connection status

#### Task 4: Device Tracker (Core 0, Priority 1)
- **Purpose:** Smart device tracking and publishing
- **Runs:** Every 5 seconds
- **Functions:**
  - Track device data changes
  - Publish only when data changes
  - 12-hour keepalive pings
  - Remove expired devices (>12h without updates)

### 12-Hour Change Detection Logic

The core intelligence of this system is in `device_tracker.h`:

#### Rules:
1. **New Device Discovered** → Publish immediately
2. **Data Changed** → Publish immediately (temperature ±0.5°C, humidity ±2%, battery ±5%)
3. **No Change** → Wait 12 hours, then publish keepalive
4. **No Updates for 12h** → Remove device from tracking

#### Data Structure:
```cpp
struct TrackedDevice {
    String macAddress;
    String name;
    String sensorType;
    
    float temperature, humidity;
    int battery, rssi;
    
    float lastTemperature, lastHumidity;
    int lastBattery;
    
    unsigned long lastUpdate;    // Last seen
    unsigned long lastPublish;   // Last published
    unsigned long lastChange;    // Last changed
    
    bool needsPublish;
    bool hasChanged;
};
```

## Configuration

### Web Portal (AP Mode)
When no configuration exists or WiFi fails:
- **SSID:** `BLE-Gateway-Setup`
- **Password:** `12345678`
- **IP:** `192.168.4.1`
- **Configure:** WiFi credentials and MQTT settings

### Remote Configuration
After WiFi connection, device fetches from:
```
GET http://gwconfig.hoptech.co.nz/{device_id}
Headers:
  X-Device-ID: {device_id}
  X-Firmware-Version: 2.0.0
```

**Expected Response (JSON or YAML):**
```json
{
  "development": "production",
  "firmware": "http://firmware.hoptech.co.nz/gateway-v2.bin",
  "company": "Hoptech",
  "mqtt_host": "mqtt.hoptech.co.nz",
  "mqtt_user": "{device_id}",
  "mqtt_password": "{hashed_device_id}",
  "timestamp": 1731175000
}
```

**Features:**
- Can override MQTT credentials
- Can provide timestamp (if NTP fails)
- Device-specific configuration based on device ID
- Optional firmware update URL

## MQTT Topics

### Publishing

**Device Data:**
```
Topic: gateway/{device_id}/device/{device_mac}
Payload: {
  "mac": "AA:BB:CC:DD:EE:FF",
  "name": "Sensor-01",
  "type": "MOKO_L02S",
  "temperature": 22.5,
  "humidity": 45.2,
  "battery": 3000,
  "rssi": -65,
  "timestamp": 1731175000,
  "changed": true
}
```

**Gateway Status:**
```
Topic: gateway/{device_id}/status
Payload: {
  "device_id": "A1B2C3D4E5F6",
  "firmware": "2.0.0",
  "uptime": 3600,
  "free_heap": 180000,
  "wifi_rssi": -45,
  "timestamp": 1731175000
}
```

### Subscribing

**Commands:**
```
Topic: gateway/{device_id}/command
```

## Key Features

### ✅ Fresh Start
- Clean PlatformIO project structure
- No legacy Arduino .ino files
- Modular header-based architecture

### ✅ XIAO ESP32-S3 Optimized
- Correct board configuration
- PSRAM support
- USB CDC enabled

### ✅ Smart Device Tracking
- Only publish when data changes
- 12-hour keepalive for unchanged devices
- Automatic device expiry
- Configurable change thresholds

### ✅ Robust Configuration
1. Flash storage (Preferences API)
2. Web portal (AP mode)
3. Remote config server
4. NTP time sync with fallback

### ✅ Multi-Threaded
- 4 dedicated FreeRTOS tasks
- Mutex-protected shared resources
- Core affinity for optimal performance

### ✅ Sensor Support
- MOKO L02S-EA01 (Temperature, Humidity, Battery)
- MOKO T&H Series (Temperature, Humidity, Battery)
- Extensible parser architecture

## Building and Uploading

### Prerequisites
```bash
pip install platformio
```

### Build
```bash
cd BLE-Gateway
platformio run
```

### Upload
```bash
platformio run --target upload
```

### Monitor
```bash
platformio device monitor
```

## Next Steps

Before this code will work, you need to:

1. **Install PlatformIO:**
   ```bash
   pip install platformio
   ```

2. **Create Config Server:**
   - Set up endpoint at `gwconfig.hoptech.co.nz/{device_id}`
   - Return JSON with configuration
   - Optionally use device_id for authentication

3. **Configure MQTT Broker:**
   - Ensure mqtt.hoptech.co.nz is accessible
   - Set up authentication if using mqtt_user/mqtt_password
   - Create topics: `gateway/+/device/+`, `gateway/+/status`, `gateway/+/command`

4. **Test Compilation:**
   ```bash
   platformio run
   ```

5. **Flash to XIAO ESP32-S3:**
   ```bash
   platformio run --target upload
   ```

## Customization

### Change Detection Thresholds
Edit in `device_tracker.h`:
```cpp
const float TEMP_THRESHOLD = 0.5;   // °C
const float HUM_THRESHOLD = 2.0;    // %
const int BATTERY_THRESHOLD = 5;    // % or mV
```

### Keepalive Interval
Edit in `device_tracker.h`:
```cpp
const unsigned long TWELVE_HOURS = 12 * 60 * 60 * 1000;
```

### Scan Timing
Edit in `ble_scanner.h`:
```cpp
const int SCAN_TIME = 5;      // seconds per scan
const int SCAN_INTERVAL = 10; // seconds between scans
```

## Device ID

The device ID is generated from the WiFi MAC address:
```cpp
device_id = "A1B2C3D4E5F6"  // 12 hex characters, uppercase
```

This is used for:
- MQTT client ID: `BLE-Gateway-{device_id}`
- Config URL: `gwconfig.hoptech.co.nz/{device_id}`
- MQTT topics: `gateway/{device_id}/...`

## Status Indicators

Monitor serial output (115200 baud) for:
- ✓ Success messages
- ✗ Error messages
- Device discoveries
- Data changes
- Publish confirmations

---

**Version:** 2.0.0  
**Target:** Seeed XIAO ESP32-S3  
**Author:** Matt Burton  
**Date:** November 2024
