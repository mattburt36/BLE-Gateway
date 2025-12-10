/**
 * Device Tracker
 * 
 * Handles:
 * - Tracking discovered BLE devices
 * - 12-hour change detection
 * - Device data comparison
 * - Automatic device expiry
 * - MQTT publishing queue
 */

#ifndef DEVICE_TRACKER_H
#define DEVICE_TRACKER_H

#include <map>
#include <ArduinoJson.h>

extern SemaphoreHandle_t deviceMapMutex;
extern unsigned long current_timestamp;

// Device tracking structure
struct TrackedDevice {
    String macAddress;
    String name;
    String sensorType;
    bool isSensor;  // True if sensor beacon with parsed temp/humidity
    
    // Current values
    float temperature;
    float humidity;
    int battery;
    int rssi;
    
    // Previous values for change detection
    float lastTemperature;
    float lastHumidity;
    int lastBattery;
    
    // Timestamps
    unsigned long lastUpdate;      // Last time we saw this device
    unsigned long lastPublish;     // Last time we published data
    unsigned long lastChange;      // Last time data changed
    
    // Flags
    bool needsPublish;
    bool hasChanged;
};

std::map<String, TrackedDevice> deviceMap;

const unsigned long SIX_HOURS = 6 * 60 * 60 * 1000; // 6 hours in milliseconds
const float TEMP_THRESHOLD = 0.1; // °C - minimum change to consider significant (reduced for testing)
const float HUM_THRESHOLD = 0.5;  // % - minimum change to consider significant (reduced for testing)
const int BATTERY_THRESHOLD = 5;  // % or mV - minimum change to consider significant

bool hasSignificantChange(const TrackedDevice& device, float newTemp, float newHum, int newBatt) {
    bool tempChanged = abs(newTemp - device.lastTemperature) >= TEMP_THRESHOLD;
    bool humChanged = abs(newHum - device.lastHumidity) >= HUM_THRESHOLD;
    bool battChanged = abs(newBatt - device.lastBattery) >= BATTERY_THRESHOLD;
    
    return tempChanged || humChanged || battChanged;
}

void updateDevice(const String& mac, const String& name, const String& type, 
                  float temp, float hum, int batt, int rssi, bool isSensor = false) {
    
    if (xSemaphoreTake(deviceMapMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        unsigned long now = millis();
        
        // Check if device exists in map
        auto it = deviceMap.find(mac);
        
        if (it == deviceMap.end()) {
            // New device - add to map
            TrackedDevice newDevice;
            newDevice.macAddress = mac;
            newDevice.name = name;
            newDevice.sensorType = type;
            newDevice.isSensor = isSensor;
            newDevice.temperature = temp;
            newDevice.humidity = hum;
            newDevice.battery = batt;
            newDevice.rssi = rssi;
            newDevice.lastTemperature = temp;
            newDevice.lastHumidity = hum;
            newDevice.lastBattery = batt;
            newDevice.lastUpdate = now;
            newDevice.lastPublish = 0;
            newDevice.lastChange = now;
            newDevice.needsPublish = true;  // Always publish new devices
            newDevice.hasChanged = false;
            
            deviceMap[mac] = newDevice;
            
            Serial.printf("New device discovered: %s (%s)\n", mac.c_str(), name.c_str());
            if (isSensor) {
                Serial.printf("  Type: %s, Temp: %.2f°C, Humidity: %.2f%%, Battery: %d, RSSI: %d\n",
                             type.c_str(), temp, hum, batt, rssi);
            } else {
                Serial.printf("  Type: %s, RSSI: %d\n", type.c_str(), rssi);
            }
        } else {
            // Existing device - update data
            TrackedDevice& device = it->second;
            device.lastUpdate = now;
            device.rssi = rssi; // Always update RSSI
            
            // Only check for changes if it's a sensor device with sensor data
            if (isSensor && hasSignificantChange(device, temp, hum, batt)) {
                Serial.printf("Device changed: %s (%s)\n", mac.c_str(), name.c_str());
                Serial.printf("  Old: Temp=%.2f°C, Hum=%.2f%%, Batt=%d\n", 
                             device.temperature, device.humidity, device.battery);
                Serial.printf("  New: Temp=%.2f°C, Hum=%.2f%%, Batt=%d\n", 
                             temp, hum, batt);
                
                // Update stored values
                device.lastTemperature = device.temperature;
                device.lastHumidity = device.humidity;
                device.lastBattery = device.battery;
                
                device.temperature = temp;
                device.humidity = hum;
                device.battery = batt;
                
                device.lastChange = now;
                device.needsPublish = true;
                device.hasChanged = true;
            } else {
                // No significant change - check if 6h elapsed
                if (now - device.lastPublish >= SIX_HOURS) {
                    Serial.printf("6h keepalive for: %s (%s)\n", mac.c_str(), name.c_str());
                    device.needsPublish = true;
                    device.hasChanged = false;
                }
            }
        }
        
        xSemaphoreGive(deviceMapMutex);
    }
}

void removeExpiredDevices() {
    if (xSemaphoreTake(deviceMapMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        unsigned long now = millis();
        
        auto it = deviceMap.begin();
        while (it != deviceMap.end()) {
            if (now - it->second.lastUpdate >= SIX_HOURS) {
                Serial.printf("Removing expired device: %s (%s)\n", 
                             it->second.macAddress.c_str(), 
                             it->second.name.c_str());
                it = deviceMap.erase(it);
            } else {
                ++it;
            }
        }
        
        xSemaphoreGive(deviceMapMutex);
    }
}

void publishPendingDevices() {
    if (xSemaphoreTake(deviceMapMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (auto& pair : deviceMap) {
            TrackedDevice& device = pair.second;
            
            if (device.needsPublish) {
                // Create JSON payload
                JsonDocument doc;
                doc["mac"] = device.macAddress;
                doc["name"] = device.name;
                doc["type"] = device.sensorType;
                doc["rssi"] = device.rssi;
                
                // Calculate timestamp - use current synced time IN MILLISECONDS for ThingsBoard
                // current_timestamp is already the current time from NTP, no need to add uptime
                // Must use unsigned long long (64-bit) to avoid overflow
                unsigned long long ts_millis = (unsigned long long)current_timestamp * 1000ULL;
                doc["timestamp"] = ts_millis;
                doc["changed"] = device.hasChanged;
                
                // Only include sensor data for sensor devices
                if (device.isSensor) {
                    doc["temperature"] = device.temperature;
                    doc["humidity"] = device.humidity;
                    // Only include battery if it's non-zero (LOP001 has no battery)
                    if (device.battery > 0) {
                        doc["battery"] = device.battery;
                    }
                }
                // Don't include null values for non-sensor devices
                
                // MQTT publishing disabled for now - just mark as published
                device.lastPublish = millis();
                device.needsPublish = false;
                device.hasChanged = false;
                
                Serial.printf("Device tracked: %s\n", device.macAddress.c_str());
            }
        }
        
        xSemaphoreGive(deviceMapMutex);
    }
}

void deviceTrackerTask(void* parameter) {
    Serial.println("Device Tracker Task started");
    
    unsigned long lastCleanup = 0;
    const unsigned long CLEANUP_INTERVAL = 60000; // 1 minute
    
    while (true) {
        // Publish any devices that need publishing
        publishPendingDevices();
        
        // Periodically remove expired devices
        if (millis() - lastCleanup > CLEANUP_INTERVAL) {
            removeExpiredDevices();
            lastCleanup = millis();
        }
        
        // Update timestamp (if time is synced)
        if (time_synced) {
            unsigned long old_ts = current_timestamp;
            current_timestamp = time(nullptr);
            
            // Debug output every minute to verify time sync is working
            static unsigned long last_debug = 0;
            if (millis() - last_debug > 60000) {
                Serial.printf("🕐 Time sync: current_timestamp=%lu (was %lu)\n", current_timestamp, old_ts);
                last_debug = millis();
            }
        } else {
            // Warning if time is not synced
            static unsigned long last_warning = 0;
            if (millis() - last_warning > 60000) {
                Serial.println("⚠️  WARNING: Time not synced! Timestamps will be incorrect.");
                last_warning = millis();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Run every 5 seconds
    }
}

#endif // DEVICE_TRACKER_H
