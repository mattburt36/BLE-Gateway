#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>

// Forward declaration
struct OTAState;

// BLE scan configuration
const int SCAN_TIME = 5;

// BLE Advertisement Entry structure
struct BLEAdvertEntry {
    uint64_t ts;
    int rssi;
    String name;
    String mfgData;
    String svcData;
    // Parsed sensor data
    bool hasTempHumidity;
    float temperature;
    float humidity;
    uint16_t battery_mv;
    String deviceType;
    // Deduplication tracking
    uint32_t dataHash;
};

// External references
extern BLEScan* pBLEScan;
extern std::map<String, BLEAdvertEntry> detectionBuffer;
extern SemaphoreHandle_t detectionBufferMutex;
extern bool config_mode;
extern OTAState otaState;

// Convert String to hex representation for JSON
String toHex(const String& data) {
    String hex = "";
    for (size_t i = 0; i < data.length(); ++i) {
        uint8_t b = data[i];
        if (b < 16) hex += "0";
        hex += String(b, HEX);
    }
    return hex;
}

// Simple hash function for deduplication
uint32_t simpleHash(const String& data) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < data.length(); i++) {
        hash = ((hash << 5) + hash) + (uint8_t)data[i];
    }
    return hash;
}

// ==== HOPTECH/MOKO L02S PARSER ====
bool parseHoptechSensor(const String& svcData, const String& svcUUID, BLEAdvertEntry& entry) {
    if (svcData.length() < 21) {
        return false;
    }
    
    if (!svcUUID.startsWith("0000ea01")) {
        return false;
    }
    
    Serial.println(">>> Hoptech/MOKO L02S Sensor Detected! <<<");
    
    uint16_t temp_raw = ((uint8_t)svcData[12] << 8) | (uint8_t)svcData[13];
    entry.temperature = temp_raw * 0.1;
    
    uint16_t hum_raw = ((uint8_t)svcData[14] << 8) | (uint8_t)svcData[15];
    entry.humidity = hum_raw * 0.1;
    
    entry.battery_mv = ((uint8_t)svcData[16] << 8) | (uint8_t)svcData[17];
    
    entry.deviceType = "Hoptech/L02S-EA01";
    entry.hasTempHumidity = true;
    
    Serial.printf("  Temperature: %.1f°C\n", entry.temperature);
    Serial.printf("  Humidity: %.1f%%\n", entry.humidity);
    Serial.printf("  Battery: %u mV\n", entry.battery_mv);
    
    return true;
}

// ==== MOKO T&H PARSER ====
bool parseMokoTH(const String& svcData, BLEAdvertEntry& entry) {
    if (svcData.length() < 18) {
        return false;
    }
    
    uint16_t serviceUuid = ((uint8_t)svcData[1] << 8) | (uint8_t)svcData[0];
    if (serviceUuid != 0xFEAB) {
        return false;
    }
    
    uint8_t frameType = (uint8_t)svcData[2];
    if (frameType != 0x70) {
        return false;
    }
    
    Serial.println(">>> MOKO T&H Sensor Detected! <<<");
    
    int16_t temp_raw = ((uint8_t)svcData[6] << 8) | (uint8_t)svcData[5];
    entry.temperature = temp_raw * 0.1;
    
    uint16_t hum_raw = ((uint8_t)svcData[8] << 8) | (uint8_t)svcData[7];
    entry.humidity = hum_raw * 0.1;
    
    entry.battery_mv = ((uint8_t)svcData[10] << 8) | (uint8_t)svcData[9];
    
    uint8_t devType = (uint8_t)svcData[11];
    switch(devType) {
        case 0x01: entry.deviceType = "MOKO-3-axis"; break;
        case 0x02: entry.deviceType = "MOKO-TH"; break;
        case 0x03: entry.deviceType = "MOKO-3-axis-TH"; break;
        default: entry.deviceType = "MOKO-Unknown"; break;
    }
    
    entry.hasTempHumidity = true;
    
    Serial.printf("  Temperature: %.1f°C\n", entry.temperature);
    Serial.printf("  Humidity: %.1f%%\n", entry.humidity);
    Serial.printf("  Battery: %d mV\n", entry.battery_mv);
    Serial.printf("  Device Type: %s\n", entry.deviceType.c_str());
    
    return true;
}

void processAdvert(BLEAdvertisedDevice& dev) {
    String mac = dev.getAddress().toString().c_str();
    int rssi = dev.getRSSI();
    String name = dev.haveName() ? dev.getName().c_str() : "";
    
    String svcDataStd;
    String svcUUIDStr = "";
    if (dev.haveServiceData()) {
        svcDataStd = dev.getServiceData();
        BLEUUID svcUUID = dev.getServiceDataUUID();
        svcUUIDStr = String(svcUUID.toString().c_str());
    }
    
    uint32_t currentHash = simpleHash(svcDataStd);
    
    // Thread-safe check for duplicates
    bool isDuplicate = false;
    if (detectionBufferMutex != NULL && xSemaphoreTake(detectionBufferMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
        if (detectionBuffer.find(mac) != detectionBuffer.end()) {
            if (detectionBuffer[mac].dataHash == currentHash) {
                detectionBuffer[mac].rssi = rssi;
                detectionBuffer[mac].ts = millis();
                isDuplicate = true;
                Serial.printf("Duplicate data for %s (RSSI updated to %d dBm)\n", mac.c_str(), rssi);
            } else {
                Serial.printf("*** Data changed for %s ***\n", mac.c_str());
            }
        }
        xSemaphoreGive(detectionBufferMutex);
        
        if (isDuplicate) return;
    }
    
    Serial.println("\n========== BLE Advertisement Detected ==========");
    Serial.printf("MAC Address: %s\n", mac.c_str());
    Serial.printf("RSSI: %d dBm\n", rssi);
    Serial.printf("Name: %s\n", name.length() > 0 ? name.c_str() : "(no name)");
    
    String mfgDataStr = "";
    if (dev.haveManufacturerData()) {
        String mfgData = dev.getManufacturerData();
        Serial.printf("Manufacturer Data Length: %d bytes\n", mfgData.length());
        Serial.print("Manufacturer Data (hex): ");
        for (size_t i = 0; i < mfgData.length(); i++) {
            Serial.printf("%02X ", (uint8_t)mfgData[i]);
        }
        Serial.println();
        mfgDataStr = String(mfgData.c_str());
    }
    
    String svcDataStr = "";
    if (dev.haveServiceData()) {
        Serial.println("Service Data present:");
        Serial.printf("Service Data Length: %d bytes\n", svcDataStd.length());
        Serial.print("Service Data (hex): ");
        for (size_t i = 0; i < svcDataStd.length(); i++) {
            Serial.printf("%02X ", (uint8_t)svcDataStd[i]);
        }
        Serial.println();
        Serial.printf("Service UUID: %s\n", svcUUIDStr.c_str());
        svcDataStr = String(svcDataStd.c_str());
    }
    
    Serial.println("===============================================\n");
    
    BLEAdvertEntry entry = {};
    entry.ts = millis();
    entry.rssi = rssi;
    entry.name = name;
    entry.mfgData = mfgDataStr;
    entry.svcData = svcDataStr;
    entry.hasTempHumidity = false;
    entry.temperature = 0.0;
    entry.humidity = 0.0;
    entry.battery_mv = 0;
    entry.deviceType = "";
    entry.dataHash = currentHash;
    
    if (dev.haveServiceData()) {
        if (!parseHoptechSensor(svcDataStd, svcUUIDStr, entry)) {
            parseMokoTH(svcDataStd, entry);
        }
    }

    // Thread-safe access to detection buffer
    if (detectionBufferMutex != NULL) {
        if (xSemaphoreTake(detectionBufferMutex, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
            detectionBuffer[mac] = entry;
            xSemaphoreGive(detectionBufferMutex);
            Serial.printf("Queued: %s RSSI:%d Name:%s\n", mac.c_str(), rssi, name.c_str());
        } else {
            Serial.println("Failed to acquire mutex for detection buffer");
        }
    } else {
        // Fallback if mutex not initialized (shouldn't happen in normal operation)
        detectionBuffer[mac] = entry;
        Serial.printf("Queued: %s RSSI:%d Name:%s\n", mac.c_str(), rssi, name.c_str());
    }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        processAdvert(advertisedDevice);
    }
};

// ==== FREERTOS BLE TASK ====
// Task 2: BLE scanning - handles Bluetooth scanning
void bleScanTask(void* parameter) {
    Serial.println("BLE Scan Task started");
    
    for(;;) {
        if (config_mode || otaState.updateInProgress) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        
        Serial.println("Scanning BLE...");
        pBLEScan->start(SCAN_TIME, false);
        
        // Wait 10 seconds between scans
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void initBLEScanner() {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

#endif // BLE_SCANNER_H
