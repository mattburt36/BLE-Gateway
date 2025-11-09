/**
 * BLE Scanner
 * 
 * Handles:
 * - Continuous BLE scanning
 * - Advertisement parsing
 * - Sensor data extraction (MOKO L02S, MOKO T&H)
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
const int SCAN_TIME = 5; // seconds
const int SCAN_INTERVAL = 10; // seconds between scans

// Parse MOKO L02S sensor data
bool parseMOKOL02S(BLEAdvertisedDevice advertisedDevice, float& temperature, float& humidity, int& battery) {
    if (!advertisedDevice.haveServiceData()) {
        return false;
    }
    
    std::string serviceData = advertisedDevice.getServiceData();
    
    if (serviceData.length() < 6) {
        return false;
    }
    
    uint8_t* data = (uint8_t*)serviceData.c_str();
    
    // Temperature (2 bytes, signed, 0.01°C resolution)
    int16_t temp_raw = (data[0] | (data[1] << 8));
    temperature = temp_raw * 0.01;
    
    // Humidity (2 bytes, unsigned, 0.01% resolution)
    uint16_t hum_raw = (data[2] | (data[3] << 8));
    humidity = hum_raw * 0.01;
    
    // Battery (2 bytes, millivolts)
    battery = (data[4] | (data[5] << 8));
    
    return true;
}

// Parse MOKO T&H sensor data
bool parseMOKOTH(BLEAdvertisedDevice advertisedDevice, float& temperature, float& humidity, int& battery) {
    if (!advertisedDevice.haveServiceData()) {
        return false;
    }
    
    std::string serviceData = advertisedDevice.getServiceData();
    
    if (serviceData.length() < 13) {
        return false;
    }
    
    uint8_t* data = (uint8_t*)serviceData.c_str();
    
    // Check frame type (should be 0x70 for T&H)
    if (data[0] != 0x70) {
        return false;
    }
    
    // Temperature (2 bytes, signed, 0.1°C resolution)
    int16_t temp_raw = (data[7] | (data[8] << 8));
    temperature = temp_raw * 0.1;
    
    // Humidity (2 bytes, unsigned, 0.1% resolution)
    uint16_t hum_raw = (data[9] | (data[10] << 8));
    humidity = hum_raw * 0.1;
    
    // Battery (1 byte, percentage)
    battery = data[11];
    
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String macAddress = advertisedDevice.getAddress().toString().c_str();
        macAddress.toUpperCase();
        
        int rssi = advertisedDevice.getRSSI();
        String name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "Unknown";
        
        float temperature = 0.0;
        float humidity = 0.0;
        int battery = 0;
        String sensorType = "UNKNOWN";
        bool hasSensorData = false;
        
        // Try to parse as MOKO L02S
        if (parseMOKOL02S(advertisedDevice, temperature, humidity, battery)) {
            sensorType = "MOKO_L02S";
            hasSensorData = true;
        }
        // Try to parse as MOKO T&H
        else if (parseMOKOTH(advertisedDevice, temperature, humidity, battery)) {
            sensorType = "MOKO_TH";
            hasSensorData = true;
        }
        
        // Update device tracker
        if (hasSensorData) {
            updateDevice(macAddress, name, sensorType, temperature, humidity, battery, rssi);
        }
    }
};

void initBLEScanner() {
    Serial.println("Initializing BLE scanner...");
    
    BLEDevice::init("BLE-Gateway");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    Serial.println("✓ BLE scanner initialized");
}

void bleScanTask(void* parameter) {
    Serial.println("BLE Scan Task started");
    
    while (true) {
        Serial.println("Starting BLE scan...");
        
        BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
        int deviceCount = foundDevices.getCount();
        
        Serial.printf("BLE scan complete. Found %d devices.\n", deviceCount);
        
        pBLEScan->clearResults();
        
        // Wait before next scan
        vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL * 1000));
    }
}

#endif // BLE_SCANNER_H
