/**
 * Offline Storage
 * 
 * Handles:
 * - Storing LOP001 detections to SPIFFS when offline
 * - Publishing stored detections when connection restored
 * - SPIFFS file system management
 */

#ifndef OFFLINE_STORAGE_H
#define OFFLINE_STORAGE_H

#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

extern bool mqtt_connected;
extern String device_id;
extern PubSubClient mqttClient;

const int MAX_OFFLINE_RECORDS = 7000;  // Maximum records (~630KB of 896KB SPIFFS)
const char* OFFLINE_DIR = "/offline";
const char* OFFLINE_INDEX = "/offline/index.txt";

void initOfflineStorage() {
    // Mount SPIFFS
    if (!SPIFFS.begin(true)) {  // true = format on fail
        Serial.println("⚠️  Failed to mount SPIFFS");
        return;
    }
    
    // Create offline directory if it doesn't exist
    if (!SPIFFS.exists(OFFLINE_DIR)) {
        SPIFFS.mkdir(OFFLINE_DIR);
    }
    
    // Count existing records
    int count = 0;
    if (SPIFFS.exists(OFFLINE_INDEX)) {
        File indexFile = SPIFFS.open(OFFLINE_INDEX, "r");
        if (indexFile) {
            count = indexFile.parseInt();
            indexFile.close();
        }
    }
    
    // Show SPIFFS usage
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    
    Serial.printf("✓ Offline storage initialized (SPIFFS)\n");
    Serial.printf("  Records pending: %d\n", count);
    Serial.printf("  SPIFFS: %d KB used / %d KB total\n", usedBytes / 1024, totalBytes / 1024);
}

// Store a LOP001 detection to SPIFFS
void storeOfflineDetection(const String& macAddress, float temperature, float humidity, int rssi, unsigned long timestamp) {
    if (mqtt_connected) {
        return;  // Don't store if we're online
    }
    
    // Read current count
    int count = 0;
    if (SPIFFS.exists(OFFLINE_INDEX)) {
        File indexFile = SPIFFS.open(OFFLINE_INDEX, "r");
        if (indexFile) {
            count = indexFile.parseInt();
            indexFile.close();
        }
    }
    
    if (count >= MAX_OFFLINE_RECORDS) {
        Serial.println("⚠️  Offline storage full, dropping oldest record");
        // Delete oldest record (index 0)
        String oldestFile = String(OFFLINE_DIR) + "/0.json";
        SPIFFS.remove(oldestFile);
        
        // Shift all files down (expensive but rare)
        for (int i = 1; i < count; i++) {
            String oldName = String(OFFLINE_DIR) + "/" + String(i) + ".json";
            String newName = String(OFFLINE_DIR) + "/" + String(i - 1) + ".json";
            SPIFFS.rename(oldName, newName);
        }
        count--;
    }
    
    // Create JSON record
    JsonDocument doc;
    doc["mac"] = macAddress;
    doc["temp"] = temperature;
    doc["hum"] = humidity;
    doc["rssi"] = rssi;
    doc["ts"] = timestamp;
    
    // Write to file
    String filename = String(OFFLINE_DIR) + "/" + String(count) + ".json";
    File file = SPIFFS.open(filename, "w");
    if (!file) {
        Serial.printf("⚠️  Failed to create file: %s\n", filename.c_str());
        return;
    }
    
    serializeJson(doc, file);
    file.close();
    
    // Update count
    File indexFile = SPIFFS.open(OFFLINE_INDEX, "w");
    if (indexFile) {
        indexFile.println(count + 1);
        indexFile.close();
    }
    
    Serial.printf("💾 Stored offline: %s (%.2f°C, %.2f%%) [%d/%d records]\n", 
                 macAddress.c_str(), temperature, humidity, count + 1, MAX_OFFLINE_RECORDS);
}

// Publish all stored offline detections
int publishOfflineDetections() {
    if (!mqtt_connected) {
        return 0;  // Can't publish if offline
    }
    
    int count = 0;
    if (SPIFFS.exists(OFFLINE_INDEX)) {
        File indexFile = SPIFFS.open(OFFLINE_INDEX, "r");
        if (indexFile) {
            count = indexFile.parseInt();
            indexFile.close();
        }
    }
    
    if (count == 0) {
        return 0;  // Nothing to publish
    }
    
    Serial.printf("\n📤 Publishing %d offline detections...\n", count);
    int published = 0;
    
    for (int i = 0; i < count; i++) {
        String filename = String(OFFLINE_DIR) + "/" + String(i) + ".json";
        
        if (!SPIFFS.exists(filename)) {
            continue;  // Skip missing files
        }
        
        File file = SPIFFS.open(filename, "r");
        if (!file) {
            Serial.printf("⚠️  Failed to open file %d\n", i);
            continue;
        }
        
        // Parse JSON record
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        
        if (error) {
            Serial.printf("⚠️  Failed to parse record %d\n", i);
            SPIFFS.remove(filename);  // Remove corrupt file
            continue;
        }
        
        // Extract data
        String macAddress = doc["mac"].as<String>();
        float temperature = doc["temp"];
        float humidity = doc["hum"];
        int rssi = doc["rssi"];
        unsigned long timestamp = doc["ts"];
        
        // Publish to MQTT (same format as live detections)
        String topic = "sensor/data";
        JsonDocument pubDoc;
        pubDoc["serialNumber"] = macAddress;
        pubDoc["sensorType"] = "LOP001";
        pubDoc["sensorModel"] = "LOP001";
        pubDoc["temp"] = String(temperature, 2);
        pubDoc["hum"] = String(humidity, 2);
        pubDoc["battery"] = 0;
        pubDoc["rssi"] = rssi;
        pubDoc["gateway"] = device_id;
        pubDoc["timestamp"] = timestamp;
        pubDoc["offline"] = true;  // Mark as offline detection
        
        String payload;
        serializeJson(pubDoc, payload);
        
        if (mqttClient.publish(topic.c_str(), payload.c_str())) {
            Serial.printf("   ✓ Published: %s (%.2f°C, %.2f%%)\n", 
                         macAddress.c_str(), temperature, humidity);
            published++;
            SPIFFS.remove(filename);  // Delete after successful publish
        } else {
            Serial.printf("   ✗ Failed: %s\n", macAddress.c_str());
            break;  // Stop if publish fails
        }
        
        delay(100);  // Small delay between publishes
    }
    
    // Clear index if all published
    if (published > 0) {
        // Reset index to 0
        File indexFile = SPIFFS.open(OFFLINE_INDEX, "w");
        if (indexFile) {
            indexFile.println(0);
            indexFile.close();
        }
        Serial.printf("✓ Published %d/%d offline detections, storage cleared\n", published, count);
    }
    
    return published;
}

// Get count of pending offline records
int getOfflineRecordCount() {
    if (!SPIFFS.exists(OFFLINE_INDEX)) {
        return 0;
    }
    
    File indexFile = SPIFFS.open(OFFLINE_INDEX, "r");
    if (!indexFile) {
        return 0;
    }
    
    int count = indexFile.parseInt();
    indexFile.close();
    return count;
}

// Clear all offline records (for manual cleanup)
void clearOfflineStorage() {
    int count = getOfflineRecordCount();
    
    for (int i = 0; i < count; i++) {
        String filename = String(OFFLINE_DIR) + "/" + String(i) + ".json";
        SPIFFS.remove(filename);
    }
    
    File indexFile = SPIFFS.open(OFFLINE_INDEX, "w");
    if (indexFile) {
        indexFile.println(0);
        indexFile.close();
    }
    
    Serial.println("✓ Offline storage cleared");
}

#endif // OFFLINE_STORAGE_H
