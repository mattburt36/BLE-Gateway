/**
 * BLE Scanner
 * 
 * Handles:
 * - Continuous BLE scanning
 * - Advertisement parsing
 * - Sensor data extraction (LOP001 Temperature Beacon)
 * - Device detection and buffering
 */

#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "device_tracker.h"

extern SemaphoreHandle_t deviceMapMutex;

BLEScan* pBLEScan = nullptr;
const int SCAN_TIME = 20; // seconds - increased to match LOP001 advertising interval
const int SCAN_INTERVAL = 5; // seconds between scans - reduced to catch more advertisements

// Parse LOP001 Temperature Beacon data
// Device Name: LOP001
// Service UUID: 0x181A (Environmental Sensing Service)
// Service Data Format:
//   Bytes 0-1: Service UUID 0x181A (little-endian)
//   Bytes 2-3: Temperature (sint16, little-endian, 0.01°C resolution)
//   Bytes 4-5: Humidity (uint16, little-endian, 0.01%RH resolution)
bool parseLOP001(BLEAdvertisedDevice advertisedDevice, float& temperature, float& humidity) {
    // Check device name
    if (!advertisedDevice.haveName() || advertisedDevice.getName() != "LOP001") {
        return false;
    }
    
    // Check for service data with Environmental Sensing Service UUID (0x181A)
    if (!advertisedDevice.haveServiceData()) {
        return false;
    }
    
    // Verify service UUID is 0x181A (Environmental Sensing Service)
    BLEUUID svcUUID = advertisedDevice.getServiceDataUUID();
    String svcUUIDStr = String(svcUUID.toString().c_str());
    if (!svcUUIDStr.startsWith("0000181a")) {
        return false;
    }
    
    std::string serviceData = advertisedDevice.getServiceData();
    
    // ESP32 BLE library strips the UUID, so we expect 4 bytes: temp (2) + humidity (2)
    if (serviceData.length() < 4) {
        return false;
    }
    
    uint8_t* data = (uint8_t*)serviceData.c_str();
    
    // Temperature (bytes 0-1, sint16, little-endian, 0.01°C resolution)
    int16_t temp_raw = (int16_t)((uint8_t)data[1] << 8 | (uint8_t)data[0]);
    temperature = temp_raw / 100.0;
    
    // Humidity (bytes 2-3, uint16, little-endian, 0.01%RH resolution)
    uint16_t hum_raw = (uint16_t)((uint8_t)data[3] << 8 | (uint8_t)data[2]);
    humidity = hum_raw / 100.0;
    
    // Sanity checks (SHT40 sensor ranges)
    if (temperature < -40.0 || temperature > 125.0) {
        return false;
    }
    
    if (humidity < 0.0 || humidity > 100.0) {
        return false;
    }
    
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String macAddress = advertisedDevice.getAddress().toString().c_str();
        macAddress.toUpperCase();
        
        int rssi = advertisedDevice.getRSSI();
        String name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "Unknown";
        
        // Debug: Log all BLE advertisements we receive
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 10000) { // Every 10 seconds
            Serial.printf("📡 BLE callback active - seeing advertisements (last: %s)\n", macAddress.c_str());
            lastDebug = millis();
        }
        
        float temperature = 0.0;
        float humidity = 0.0;
        int battery = 0;
        String sensorType = "BLE_DEVICE";
        bool isLOP001 = false;
        
        // Try to parse as LOP001 Temperature Beacon
        if (parseLOP001(advertisedDevice, temperature, humidity)) {
            sensorType = "LOP001";
            isLOP001 = true;
            
            Serial.printf("🔍 LOP001 detected: %s RSSI=%d T=%.2f H=%.2f\n", 
                         macAddress.c_str(), rssi, temperature, humidity);
            
            // Only update device tracker for LOP001 sensors
            updateDevice(macAddress, name, sensorType, temperature, humidity, battery, rssi, isLOP001);
        }
        // Ignore all non-LOP001 devices
    }
};

void initBLEScanner() {
    Serial.println("Initializing BLE scanner...");
    
    BLEDevice::init("BLE-Gateway");
    pBLEScan = BLEDevice::getScan();
    
    // Set callbacks with wantDuplicates=true to get all advertisements
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    Serial.println("✓ BLE scanner initialized (duplicates enabled)");
}

void bleScanTask(void* parameter) {
    Serial.println("BLE Scan Task started");
    
    while (true) {
        Serial.println("Starting BLE scan...");
        
        // Stop any previous scan and clear results to fully reset duplicate filter
        pBLEScan->stop();
        pBLEScan->clearResults();
        
        // Small delay to ensure BLE stack is ready
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Start scan - second parameter false means don't delete results after scan
        BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
        int deviceCount = foundDevices.getCount();
        
        Serial.printf("BLE scan complete. Found %d devices.\n", deviceCount);
        
        // Wait before next scan
        vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL * 1000));
    }
}

#endif // BLE_SCANNER_H
