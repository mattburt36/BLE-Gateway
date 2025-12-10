/**
 * OTA Manager
 * 
 * Handles:
 * - Over-The-Air firmware updates via HTTP/HTTPS
 * - ThingsBoard attribute-based OTA updates
 * - MQTT-triggered updates
 * - Progress reporting
 * - Rollback on failure
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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
    WiFiClient* client = nullptr;
    WiFiClientSecure* secureClient = nullptr;
    bool isHttps = firmwareUrl.startsWith("https://");
    
    Serial.println("\n=== Starting OTA Update ===");
    Serial.printf("URL: %s\n", firmwareUrl.c_str());
    Serial.printf("Protocol: %s\n", isHttps ? "HTTPS" : "HTTP");
    
    otaState = OTA_DOWNLOADING;
    otaProgress = 0;
    otaError = "";
    publishOTAStatus("downloading", 0);
    
    // Setup HTTP/HTTPS client
    if (isHttps) {
        secureClient = new WiFiClientSecure();
        secureClient->setInsecure(); // Skip certificate validation (for simplicity)
        http.begin(*secureClient, firmwareUrl);
        client = secureClient;
        Serial.println("Using HTTPS (insecure mode)");
    } else {
        http.begin(firmwareUrl);
        Serial.println("Using HTTP");
    }
    
    // Set timeout
    http.setTimeout(15000);
    
    Serial.println("Sending GET request...");
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        otaError = "HTTP error: " + String(httpCode);
        Serial.printf("✗ %s\n", otaError.c_str());
        otaState = OTA_FAILED;
        publishOTAStatus("failed", 0);
        http.end();
        if (secureClient) delete secureClient;
        return false;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        otaError = "Invalid content length";
        Serial.printf("✗ %s\n", otaError.c_str());
        otaState = OTA_FAILED;
        publishOTAStatus("failed", 0);
        http.end();
        if (secureClient) delete secureClient;
        return false;
    }
    
    Serial.printf("Firmware size: %d bytes\n", contentLength);
    
    // Validate expected size if provided
    if (expectedSize > 0 && contentLength != expectedSize) {
        Serial.printf("⚠️  Warning: Expected %d bytes, got %d bytes\n", expectedSize, contentLength);
    }
    
    // Check if we have enough space
    if (!Update.begin(contentLength)) {
        otaError = "Not enough space for OTA";
        Serial.printf("✗ %s (available: %d bytes needed)\n", otaError.c_str(), contentLength);
        otaState = OTA_FAILED;
        publishOTAStatus("failed", 0);
        http.end();
        if (secureClient) delete secureClient;
        return false;
    }
    
    otaState = OTA_UPDATING;
    publishOTAStatus("updating", 0);
    
    WiFiClient* stream = http.getStreamPtr();
    
    // Use larger buffer for better performance
    const size_t bufferSize = 512;
    uint8_t* buffer = new uint8_t[bufferSize];
    int written = 0;
    int lastProgress = 0;
    unsigned long lastUpdate = millis();
    
    Serial.println("Starting firmware download...");
    
    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        
        if (available) {
            int readBytes = stream->readBytes(buffer, min(available, bufferSize));
            
            if (readBytes > 0) {
                size_t bytesWritten = Update.write(buffer, readBytes);
                
                if (bytesWritten != readBytes) {
                    otaError = "Write error";
                    Serial.printf("✗ %s (tried: %d, written: %d)\n", otaError.c_str(), readBytes, bytesWritten);
                    delete[] buffer;
                    Update.abort();
                    otaState = OTA_FAILED;
                    publishOTAStatus("failed", otaProgress);
                    http.end();
                    if (secureClient) delete secureClient;
                    return false;
                }
                
                written += bytesWritten;
                
                // Calculate progress
                otaProgress = (written * 100) / contentLength;
                
                // Report progress every 10% or every 5 seconds
                unsigned long now = millis();
                if (otaProgress >= lastProgress + 10 || (now - lastUpdate) > 5000) {
                    Serial.printf("Progress: %d%% (%d/%d bytes)\n", otaProgress, written, contentLength);
                    publishOTAStatus("updating", otaProgress);
                    lastProgress = otaProgress;
                    lastUpdate = now;
                }
            }
            
            // Yield to prevent watchdog timeout
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            delay(1);
        }
        
        // Timeout check (60 seconds without data)
        if (millis() - lastUpdate > 60000) {
            otaError = "Download timeout";
            Serial.printf("✗ %s (no data for 60 seconds)\n", otaError.c_str());
            delete[] buffer;
            Update.abort();
            otaState = OTA_FAILED;
            publishOTAStatus("failed", otaProgress);
            http.end();
            if (secureClient) delete secureClient;
            return false;
        }
    }
    
    delete[] buffer;
    http.end();
    if (secureClient) delete secureClient;
    
    if (written != contentLength) {
        otaError = "Incomplete download";
        Serial.printf("✗ %s (expected: %d, got: %d)\n", otaError.c_str(), contentLength, written);
        Update.abort();
        otaState = OTA_FAILED;
        publishOTAStatus("failed", otaProgress);
        return false;
    }
    
    Serial.println("Download complete, finalizing update...");
    
    if (Update.end(true)) {
        Serial.println("✓ OTA update completed successfully");
        Serial.printf("   Downloaded: %d bytes\n", written);
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

void handleThingsBoardAttributeUpdate(const String& payload) {
    Serial.println("📦 ThingsBoard attribute update received");
    
    if (otaState == OTA_DOWNLOADING || otaState == OTA_UPDATING) {
        Serial.println("✗ OTA already in progress");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("✗ Failed to parse attribute update: %s\n", error.c_str());
        return;
    }
    
    // ThingsBoard sends attribute updates as {"firmwareVersion":"value"}
    // Extract the firmware URL or version information
    String firmwareAttr = doc["firmwareVersion"] | "";
    
    if (firmwareAttr.length() == 0) {
        Serial.println("✗ No firmwareVersion in attribute update");
        return;
    }
    
    Serial.printf("Firmware attribute value: %s\n", firmwareAttr.c_str());
    
    // Check if it's a URL or version string
    if (firmwareAttr.startsWith("http://") || firmwareAttr.startsWith("https://")) {
        // Direct firmware URL
        Serial.printf("OTA request from ThingsBoard: URL: %s\n", firmwareAttr.c_str());
        performOTA(firmwareAttr, 0);
    } else {
        // Version string - you may need to construct URL or handle differently
        Serial.printf("Firmware version update: %s (current: %s)\n", firmwareAttr.c_str(), FIRMWARE_VERSION);
        
        if (firmwareAttr == FIRMWARE_VERSION) {
            Serial.println("Already running this version, skipping OTA");
            publishOTAStatus("up_to_date", 100);
        } else {
            Serial.println("⚠️  Firmware version specified but no URL provided");
            Serial.println("   Configure firmware URL in ThingsBoard attribute or use direct URL");
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    Serial.println("\n📨 ========== MQTT MESSAGE RECEIVED ==========");
    Serial.printf("   Topic: %s\n", topicStr.c_str());
    Serial.printf("   Length: %d bytes\n", length);
    Serial.printf("   Payload: %s\n", payloadStr.c_str());
    Serial.println("   ============================================\n");
    
    // Handle ThingsBoard attribute updates for OTA
    if (topicStr.endsWith("/firmwareVersion")) {
        Serial.println("🔄 Processing ThingsBoard OTA attribute update...");
        handleThingsBoardAttributeUpdate(payloadStr);
    }
    // Handle OTA updates
    else if (topicStr.endsWith("/ota")) {
        Serial.println("🔄 Processing OTA update message...");
        handleOTAMessage(payloadStr);
    }
    // Handle other commands
    else if (topicStr.endsWith("/command")) {
        Serial.println("⚡ Processing command message...");
        
        // Parse command
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payloadStr);
        
        if (!error) {
            String cmd = doc["command"] | "";
            Serial.printf("   Command type: %s\n", cmd.c_str());
            
            if (cmd == "restart") {
                Serial.println("♻️  Restart command received - rebooting in 1 second...");
                delay(1000);
                ESP.restart();
            } else {
                Serial.printf("⚠️  Unknown command: %s\n", cmd.c_str());
            }
        } else {
            Serial.printf("❌ Failed to parse command JSON: %s\n", error.c_str());
        }
    }
    // Handle ThingsBoard RPC requests (sensor/+/request/+/+)
    else if (topicStr.indexOf("/request/") > 0) {
        Serial.println("📞 Processing RPC request...");
        
        // Extract method name and request ID from topic
        // Format: sensor/{deviceName}/request/{methodName}/{requestId}
        int requestPos = topicStr.indexOf("/request/");
        String afterRequest = topicStr.substring(requestPos + 9);
        int slashPos = afterRequest.indexOf('/');
        String methodName = afterRequest.substring(0, slashPos);
        String requestId = afterRequest.substring(slashPos + 1);
        
        Serial.printf("   Method: %s, Request ID: %s\n", methodName.c_str(), requestId.c_str());
        
        // Handle echo method (two-way RPC)
        if (methodName == "echo") {
            String responseTopic = "sensor/" + device_id + "/response/echo/" + requestId;
            
            bool success = false;
            if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                success = mqttClient.publish(responseTopic.c_str(), payloadStr.c_str(), false);
                xSemaphoreGive(mqttMutex);
            }
            
            if (success) {
                Serial.printf("✅ RPC response sent to %s\n", responseTopic.c_str());
            } else {
                Serial.printf("❌ Failed to send RPC response\n");
            }
        }
        // Handle other RPC methods as needed
        else {
            Serial.printf("⚠️  Unhandled RPC method: %s\n", methodName.c_str());
        }
    }
    else {
        Serial.printf("⚠️  Unhandled topic: %s\n", topicStr.c_str());
    }
}

#endif // OTA_MANAGER_H
