# SenseCAP Indicator BLE Gateway - Implementation Notes

## Overview
This branch implements a BLE temperature monitor with a 4" touch display for the SenseCAP Indicator D1. It scans for BLE temperature sensors (LOP001 beacons) and displays real-time temperature/humidity data on the screen.

## Current Features (v3.0.0)

### ✅ Display System
- **RGB LCD Interface**: 480x480 pixel display via ESP32-S3 RGB parallel interface
- **LVGL Graphics**: Hardware-accelerated UI rendering with dual framebuffers in PSRAM
- **Touch Input**: FT5x06 capacitive touch controller via I2C
- **I/O Expander**: PCA9535 controls display enable, reset, and status LEDs

### ✅ BLE Scanning
- **Universal Detection**: Scans and reports all BLE devices in range
- **LOP001 Parsing**: Extracts temperature and humidity from LOP001 beacons
- **Real-time Updates**: Display updates every second with latest sensor data
- **Device Tracking**: Maintains device map with MAC, RSSI, name, and sensor values

### ✅ WiFi & Time
- **Touch Configuration**: On-screen keyboard for WiFi SSID/password entry
- **Persistent Storage**: WiFi credentials saved in encrypted NVS flash
- **NTP Sync**: Automatic time synchronization when WiFi connected
- **Time Display**: Shows current time in 12-hour format on main screen

### ✅ User Interface
**Main Screen:**
- Title header
- Current time display (updates every second)
- WiFi connection status (red when disconnected, green when connected)
- Scrollable list of detected sensors showing:
  - Device name or MAC address
  - Temperature and humidity (if available)
  - Signal strength (RSSI)

**WiFi Config Screen:**
- Automatically shown when no WiFi credentials stored
- SSID text input field
- Password text input field (masked)
- On-screen keyboard (automatically appears on field focus)
- Connect button to save and attempt connection

## Code Structure

### Main Components

**src/display_manager.h** (New)
- RGB LCD initialization using ESP32-S3 LCD peripheral
- LVGL display driver and touch input callbacks
- PCA9535 I/O expander control functions
- Main screen creation and temperature display
- WiFi configuration screen with keyboard
- UI update functions called from main loop

**src/main.cpp** (Modified)
- Removed MQTT dependencies temporarily
- Added display initialization in setup()
- Shows WiFi config screen if no credentials
- Main loop calls `update_display()` for UI refresh
- Simplified to single BLE scanning task

**src/config_manager.h** (Simplified)
- Removed MQTT configuration variables
- Keeps only WiFi credentials
- Uses encrypted NVS storage

**src/wifi_manager.h** (Unchanged)
- WiFi connection functions
- NTP time synchronization
- Timezone: UTC+13 (NZDT New Zealand)

**src/ble_scanner.h** (Unchanged)
- BLE scanning task
- LOP001 temperature beacon parsing
- Device tracking with mutex protection

**src/device_tracker.h** (Unchanged)
- DeviceData structure
- Device map management
- Thread-safe access via mutex

## Hardware Configuration

### Display Pins (ESP32-S3)
- RGB Data: GPIO0-15 (5-6-5 RGB format)
- Control: GPIO16-18, GPIO21 (HSYNC, VSYNC, DE, PCLK)
- I2C: GPIO39 (SDA), GPIO40 (SCL)
- Touch INT: GPIO42

### Memory Usage
- PSRAM: Used for LVGL framebuffers (2x 480x10 pixel buffers)
- Flash: 8MB (Huge APP partition scheme)
- RAM: ~50KB for LVGL, plus device tracking map

### Power
- 5V/1A USB-C power required
- Display backlight always on (controlled via PCA9535)

## Build Configuration (platformio.ini)

```ini
[env:sensecap_indicator]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_build.partitions = huge_app.csv
board_build.f_cpu = 240000000L
board_build.flash_size = 8MB

lib_deps = 
    lvgl/lvgl@^8.3.0
    bodmer/TFT_eSPI@^2.5.0
    bblanchon/ArduinoJson@^7.0.0

build_flags = 
    -DBOARD_HAS_PSRAM
    -DSENSECAP_INDICATOR
    -DLV_CONF_INCLUDE_SIMPLE
```

## Current Limitations / TODO

### Not Yet Implemented
- [ ] WiFi configuration screen "Connect" button handler
- [ ] Saving WiFi credentials from display UI
- [ ] WiFi reconnection logic from display
- [ ] Touch keyboard interaction (currently shows but not fully wired)
- [ ] MQTT publishing (removed for now, will add back)
- [ ] OTA updates via display
- [ ] RP2040 communication (unused in this version)
- [ ] Grove sensor support
- [ ] Data logging to SD card

### Known Issues
- Touch calibration may need adjustment per unit
- Display flicker possible if buffer size too small
- WiFi config screen needs event handlers connected

## Next Steps

### Phase 1: Complete WiFi Config UI
1. Wire up keyboard events to text areas
2. Implement "Connect" button handler:
   ```cpp
   - Get SSID from ssid_textarea
   - Get password from pass_textarea
   - Save to config (call saveConfig())
   - Attempt WiFi connection
   - Show success/error message
   - Return to main screen on success
   ```
3. Add "back" or "cancel" button
4. Test touch keyboard input

### Phase 2: Re-add MQTT
1. Add back MQTT configuration storage
2. Create MQTT settings screen (after WiFi connected)
3. Re-enable MQTT publishing task
4. Display MQTT connection status

### Phase 3: Enhanced UI
1. Add graphs for temperature history
2. Settings screen with:
   - WiFi reconfiguration
   - MQTT settings
   - Display brightness
   - Timezone adjustment
3. Status icons (WiFi, BLE, MQTT)
4. Sensor detail view (tap to expand)

### Phase 4: RP2040 Integration
1. UART communication between ESP32-S3 and RP2040
2. Offload sensor management to RP2040
3. SD card logging via RP2040
4. Buzzer alerts

## Testing Checklist

- [ ] Display initializes correctly
- [ ] Touch responds accurately
- [ ] WiFi config screen shows on first boot
- [ ] Keyboard appears on text field tap
- [ ] WiFi credentials save and persist
- [ ] BLE scanning finds LOP001 sensors
- [ ] Temperature display updates in real-time
- [ ] Time displays correctly after NTP sync
- [ ] WiFi status updates on connect/disconnect
- [ ] No memory leaks during extended operation

## Performance Notes

- Display refresh: ~60 FPS possible with RGB interface
- BLE scan interval: 20 seconds active, 5 seconds pause
- UI update rate: 1 Hz (every 1000ms)
- Touch read rate: Polled at LVGL timer rate (~5ms)
- Memory: PSRAM usage critical for display buffers

## References

- [LVGL Documentation](https://docs.lvgl.io/8.3/)
- [ESP32-S3 LCD Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html)
- [SenseCAP Indicator Wiki](https://wiki.seeedstudio.com/Sensor/SenseCAP/SenseCAP_Indicator/)
- See `/documents/` for hardware specs and setup guides
