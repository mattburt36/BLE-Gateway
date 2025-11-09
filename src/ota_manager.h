/**
 * OTA Manager
 * 
 * Handles:
 * - Over-The-Air firmware updates
 * - MQTT-triggered updates
 * - Progress reporting
 * - Rollback on failure
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Update.h>
#include <HTTPClient.h>

extern String firmware_url;
extern String device_id;
extern PubSubClient mqttClient;
extern SemaphoreHandle_t mqttMutex;

enum OTAState {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_DOWNLOADING,
    OTA_UPDATING,
    OTA_SUCCESS,
    OTA_FAILED
};

OTAState otaState = OTA_IDLE;
int otaProgress = 0;
String otaError = "";

void publishOTAStatus(const String& status, int progress = 0) {
    if (!mqttClient.connected()) {
        return;
    }
    
    String topic = "gateway/" + device_id + "/ota/status";
    
    JsonDocument doc;
    doc["device_id"] = device_id;
    doc["status"] = status;
    doc["progress"] = progress;
    doc["current_version"] = FIRMWARE_VERSION;
    
    if (otaError.length() > 0) {
        doc["error"] = otaError;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        mqttClient.publish(topic.c_str(), payload.c_str(), false);
        xSemaphoreGive(mqttMutex);
    }
}

bool performOTA(const String& firmwareUrl, int expectedSize = 0) {
    HTTPClient http;
    
    Serial.println("\n=== Starting OTA Update ===");
    Serial.printf("URL: %s\n", firmwareUrl.c_str());
    
    otaState = OTA_DOWNLOADING;
    otaProgress = 0;
    otaError = "";
    publishOTAStatus("downloading", 0);
    
    http.begin(firmwareUrl);
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        otaError = "HTTP error: " + String(httpCode);
        Serial.printf("✗ %s\n", otaError.c_str());
        otaState = OTA_FAILED;
        publishOTAStatus("failed", 0);
        http.end();
        return false;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        otaError = "Invalid content length";
        Serial.printf("✗ %s\n", otaError.c_str());
        otaState = OTA_FAILED;
        publishOTAStatus("failed", 0);
        http.end();
        return false;
    }
    
    Serial.printf("Firmware size: %d bytes\n", contentLength);
    
    // Check if we have enough space
    if (!Update.begin(contentLength)) {
        otaError = "Not enough space for OTA";
        Serial.printf("✗ %s\n", otaError.c_str());
        otaState = OTA_FAILED;
        publishOTAStatus("failed", 0);
        http.end();
        return false;
    }
    
    otaState = OTA_UPDATING;
    publishOTAStatus("updating", 0);
    
    WiFiClient* stream = http.getStreamPtr();
    
    uint8_t buffer[128];
    int written = 0;
    int lastProgress = 0;
    
    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        
        if (available) {
            int readBytes = stream->readBytes(buffer, min(available, sizeof(buffer)));
            written += Update.write(buffer, readBytes);
            
            // Calculate progress
            otaProgress = (written * 100) / contentLength;
            
            // Report progress every 10%
            if (otaProgress >= lastProgress + 10) {
                Serial.printf("Progress: %d%%\n", otaProgress);
                publishOTAStatus("updating", otaProgress);
                lastProgress = otaProgress;
            }
            
            // Yield to prevent watchdog timeout
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            delay(1);
        }
    }
    
    http.end();
    
    if (written != contentLength) {
        otaError = "Incomplete download";
        Serial.printf("✗ %s (expected: %d, got: %d)\n", otaError.c_str(), contentLength, written);
        Update.abort();
        otaState = OTA_FAILED;
        publishOTAStatus("failed", otaProgress);
        return false;
    }
    
    if (Update.end(true)) {
        Serial.println("✓ OTA update completed successfully");
        otaState = OTA_SUCCESS;
        publishOTAStatus("success", 100);
        
        Serial.println("Rebooting in 3 seconds...");
        delay(3000);
        ESP.restart();
        
        return true;
    } else {
        otaError = "Update failed: " + String(Update.errorString());
        Serial.printf("✗ %s\n", otaError.c_str());
        otaState = OTA_FAILED;
        publishOTAStatus("failed", otaProgress);
        return false;
    }
}

void handleOTAMessage(const String& payload) {
    Serial.println("OTA message received");
    
    if (otaState == OTA_DOWNLOADING || otaState == OTA_UPDATING) {
        Serial.println("✗ OTA already in progress");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("✗ Failed to parse OTA message: %s\n", error.c_str());
        return;
    }
    
    // Extract firmware info
    String version = doc["version"] | "";
    String url = doc["url"] | "";
    int size = doc["size"] | 0;
    
    if (url.length() == 0) {
        Serial.println("✗ No firmware URL provided");
        return;
    }
    
    Serial.printf("OTA request: Version %s, URL: %s\n", version.c_str(), url.c_str());
    
    // Check if we need to update
    if (version == FIRMWARE_VERSION) {
        Serial.println("Already running this version, skipping OTA");
        publishOTAStatus("up_to_date", 100);
        return;
    }
    
    // Perform OTA update
    performOTA(url, size);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    Serial.printf("MQTT message received: %s\n", topicStr.c_str());
    
    // Handle OTA updates
    if (topicStr.endsWith("/ota")) {
        handleOTAMessage(payloadStr);
    }
    // Handle other commands
    else if (topicStr.endsWith("/command")) {
        Serial.printf("Command: %s\n", payloadStr.c_str());
        
        // Parse command
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payloadStr);
        
        if (!error) {
            String cmd = doc["command"] | "";
            
            if (cmd == "restart") {
                Serial.println("Restart command received");
                delay(1000);
                ESP.restart();
            }
        }
    }
}

#endif // OTA_MANAGER_H
