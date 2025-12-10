# SenseCAP Indicator BLE Gateway - Testing Guide

## Quick Start - Upload and Test

### 1. Build and Upload

**Using PlatformIO:**
```bash
cd /home/matt/src/Hoptech/Devices/BLE-Gateway
pio run -t upload
```

**Or with monitor:**
```bash
pio run -t upload && pio device monitor
```

### 2. What Should Happen

#### First Boot (No WiFi Credentials)
1. **Display initializes** - Screen should light up
2. **WiFi Config Screen** appears with:
   - "WiFi Configuration" title
   - SSID text field
   - Password text field  
   - Connect button

#### Touch Interaction Test
1. **Tap SSID field** → Virtual keyboard should slide up from bottom
2. **Tap letters** → Should type into SSID field
3. **Tap "OK"** on keyboard → Keyboard should hide
4. **Tap Password field** → Keyboard should appear again
5. **Type password** → Should show as dots (password masked)
6. **Tap Connect button** → Should:
   - Save credentials to flash
   - Attempt WiFi connection
   - If successful: Switch to main screen
   - If failed: Stay on config screen

#### After WiFi Connects
1. **Main Screen** appears showing:
   - "BLE Temperature Monitor" title
   - Current time (after NTP sync)
   - WiFi status: "WiFi: [YourSSID]" in green
   - "Scanning for BLE devices..." message

2. **BLE Scanning starts**
   - Wait 20-30 seconds for first scan
   - Any LOP001 sensors should appear with temp/humidity
   - Other BLE devices show as "BLE_DEVICE"

3. **Display Updates**
   - Time updates every second
   - Sensor list refreshes every second
   - RSSI values update

## Serial Monitor Output

You should see messages like:
```
========================================
BLE-Gateway-SenseCAP v3.0.0
SenseCAP Indicator BLE Gateway
========================================
Device ID: [MAC ADDRESS]

Initializing display system...
Display initialized successfully
✓ Configuration manager initialized (NVS encrypted)
✗ No valid configuration found
Showing WiFi configuration screen...

Setup complete.
```

After entering WiFi credentials and pressing Connect:
```
Connect button clicked!
Attempting to connect to: YourSSID
WiFi credentials saved to flash

========== WiFi CONNECTION ATTEMPT ==========
📡 SSID: YourSSID
🔑 Password: ***SET***
⏳ Connecting.....
✅ ✅ ✅ WiFi CONNECTED! ✅ ✅ ✅
   IP Address: 192.168.1.XXX
==========================================

Synchronizing time with NTP server...
Time synchronized successfully!
Creating FreeRTOS tasks...
Initializing BLE scanner...
BLE scanner initialized
BLE task created successfully!
```

## Troubleshooting

### Display Not Working
**Symptom:** Blank screen
- Check power: 5V/1A via USB-C
- Check serial output for "Display initialization failed!"
- Verify I2C devices detected (PCA9535 at 0x20, Touch at 0x38)

**Test I2C:**
```cpp
// Add to setup() temporarily:
Wire.begin(39, 40);
Wire.beginTransmission(0x20);
if (Wire.endTransmission() == 0) {
    Serial.println("PCA9535 found!");
}
```

### Touch Not Responding
**Symptom:** Tapping does nothing
- Check touch controller I2C address (should be 0x38)
- Look for touch INT signal on GPIO42
- Add debug to `read_touch()` function:
```cpp
Serial.printf("Touch: x=%d, y=%d, touched=%d\n", x, y, touched);
```

### Keyboard Doesn't Appear
**Symptom:** Tapping text field does nothing
- Check event callback is firing (add Serial.println in `textarea_event_handler`)
- Verify keyboard object is created (not NULL)
- Check if keyboard is just hidden behind other objects

### WiFi Won't Connect
**Symptom:** Stays on config screen, doesn't connect
- Check serial monitor for error details
- Verify SSID is exactly correct (case-sensitive)
- Check password is correct
- Try 2.4GHz WiFi only (ESP32 doesn't support 5GHz)
- Check router isn't blocking new devices

### BLE Devices Not Found
**Symptom:** "No sensors detected..." stays on screen
- LOP001 sensors must be powered on and advertising
- Wait at least 30 seconds (scan interval is 20s active + 5s pause)
- Check serial: should see "BLE scan complete - found X devices"
- Test with phone's BLE scanner to verify sensors are advertising

### Time Not Showing
**Symptom:** Time says "--:--:--"
- Requires WiFi connection first
- NTP sync can take 5-10 seconds
- Check `time_synced` becomes true in serial output
- Verify NTP server is reachable (time.cloudflare.com)

## Expected Memory Usage

**Serial output after init:**
```
Free heap: ~200KB
PSRAM free: ~7.5MB (of 8MB)
Largest free block: ~180KB
```

If heap gets too low (<50KB), you may see crashes or display glitches.

## Performance Metrics

- **Display FPS**: 60 Hz capable (limited by UI complexity)
- **Touch latency**: <50ms
- **BLE scan time**: 20 seconds per cycle
- **WiFi connection**: 5-10 seconds
- **NTP sync**: 2-5 seconds
- **UI update rate**: 1 Hz (1 second intervals)

## Next Test Steps

### Test 1: Basic Display
- [ ] Screen lights up
- [ ] WiFi config screen visible
- [ ] Text is readable
- [ ] No flickering or artifacts

### Test 2: Touch Calibration
- [ ] Touch response matches finger position
- [ ] All screen areas respond (corners, edges, center)
- [ ] No "ghost touches"

### Test 3: WiFi Configuration
- [ ] Keyboard appears on tap
- [ ] Can type SSID
- [ ] Can type password (shows as dots)
- [ ] Connect button works
- [ ] Credentials save to flash
- [ ] Survives reboot (should auto-connect next time)

### Test 4: BLE Scanning
- [ ] Finds LOP001 sensors
- [ ] Shows temperature/humidity
- [ ] Updates RSSI
- [ ] Displays multiple sensors
- [ ] Sensor list scrolls if >4 devices

### Test 5: Time Display
- [ ] Shows correct time after NTP sync
- [ ] Updates every second
- [ ] 12-hour format (AM/PM)
- [ ] Correct timezone (NZDT = UTC+13)

## Advanced Testing

### Memory Leak Test
Run for 1+ hour and monitor heap:
```bash
# Add to loop():
if (millis() % 60000 == 0) {  // Every minute
    Serial.printf("Heap: %d, PSRAM: %d\n", 
                  ESP.getFreeHeap(), 
                  ESP.getFreePsram());
}
```

Heap should stay stable (±10KB variance is normal).

### Touch Stress Test
Rapidly tap different UI elements for 1 minute. Should not:
- Crash
- Freeze
- Lose touch response
- Corrupt display

### WiFi Reconnect Test
1. Connect to WiFi successfully
2. Turn off router
3. Wait for "WiFi: Disconnected" message
4. Turn router back on
5. Should auto-reconnect within 30 seconds

## Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| Compile errors about LVGL | Update `lvgl` library to v8.3.0+ |
| "Failed to allocate display buffers" | Enable PSRAM in board settings |
| Display colors wrong | Check RGB pin order (0-15) |
| Touch inverted/rotated | Adjust touch coordinate mapping |
| WiFi connects but no internet | Check router DHCP/DNS settings |
| Sensors not parsing correctly | Verify LOP001 beacon format |

## Debug Mode

To enable extra debug output, add to platformio.ini:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5  ; Maximum ESP32 debug
    -DLV_USE_LOG=1        ; LVGL debug logs
```

## Success Criteria

✅ **Ready for deployment if:**
- Display initializes reliably
- Touch is accurate
- WiFi config works first try
- Credentials persist across reboot
- BLE sensors detected within 30s
- Time syncs correctly
- No crashes after 1 hour runtime
- Memory usage stable

## Need Help?

Check serial monitor output and match against expected messages above. Most issues will show clear error messages in serial output.
