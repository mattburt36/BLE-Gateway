#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

// OTA State structure
struct OTAState {
    String firmwareTitle;
    String firmwareVersion;
    String firmwareUrl;
    int firmwareSize;
    String firmwareChecksum;
    bool updateAvailable;
    bool updateInProgress;
    int progressPercent;
    String status;
};

extern OTAState otaState;
extern bool mqtt_connected;

// Forward declarations
extern void sendOTAStatus(const String& status, int progress);
extern void mqttClientLoop();
extern void mqttClientPublish(const char* topic, const char* payload);

// ==== OTA UPDATE IMPLEMENTATION ====
bool performOTAUpdate() {
    if (!otaState.firmwareUrl.length()) {
        Serial.println("No firmware URL specified");
        sendOTAStatus("FAILED", 0);
        return false;
    }
    
    Serial.println("\n========== Starting OTA Update ==========");
    Serial.printf("Current Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("New Version: %s\n", otaState.firmwareVersion.c_str());
    Serial.printf("Firmware URL: %s\n", otaState.firmwareUrl.c_str());
    
    otaState.updateInProgress = true;
    sendOTAStatus("DOWNLOADING", 0);
    
    HTTPClient http;
    http.begin(otaState.firmwareUrl);
    http.setTimeout(30000); // 30 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP error: %d\n", httpCode);
        sendOTAStatus("FAILED", 0);
        http.end();
        otaState.updateInProgress = false;
        return false;
    }
    
    int contentLength = http.getSize();
    Serial.printf("Firmware size: %d bytes\n", contentLength);
    
    if (contentLength <= 0) {
        Serial.println("Invalid content length");
        sendOTAStatus("FAILED", 0);
        http.end();
        otaState.updateInProgress = false;
        return false;
    }
    
    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        Serial.println("Not enough space to begin OTA");
        sendOTAStatus("FAILED", 0);
        http.end();
        otaState.updateInProgress = false;
        return false;
    }
    
    Serial.println("Starting firmware download and flash...");
    sendOTAStatus("DOWNLOADING", 10);
    
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    int lastProgress = 10;
    uint8_t buff[128];
    
    while (http.connected() && written < contentLength) {
        size_t available = stream->available();
        
        if (available) {
            int toRead = min((int)available, (int)sizeof(buff));
            int bytesRead = stream->readBytes(buff, toRead);
            
            size_t bytesWritten = Update.write(buff, bytesRead);
            
            if (bytesWritten != bytesRead) {
                Serial.printf("Write error: wrote %d of %d bytes\n", bytesWritten, bytesRead);
                sendOTAStatus("FAILED", 0);
                Update.abort();
                http.end();
                otaState.updateInProgress = false;
                return false;
            }
            
            written += bytesWritten;
            
            int progress = 10 + (written * 80 / contentLength); // 10-90% for download
            if (progress > lastProgress + 5) {
                sendOTAStatus("DOWNLOADING", progress);
                Serial.printf("Progress: %d%% (%d/%d bytes)\n", progress, written, contentLength);
                lastProgress = progress;
                mqttClientLoop(); // Keep MQTT alive
            }
        }
        
        // Yield to watchdog during OTA download
        bool inTask = xTaskGetCurrentTaskHandle() != NULL;
        if (inTask) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        } else {
            delay(1);
        }
    }
    
    http.end();
    
    if (written != contentLength) {
        Serial.printf("Download incomplete: got %d bytes, expected %d\n", written, contentLength);
        sendOTAStatus("FAILED", 0);
        Update.abort();
        otaState.updateInProgress = false;
        return false;
    }
    
    Serial.println("Firmware downloaded successfully");
    sendOTAStatus("UPDATING", 95);
    
    if (!Update.end(true)) {
        Serial.printf("Update error: %s\n", Update.errorString());
        sendOTAStatus("FAILED", 0);
        otaState.updateInProgress = false;
        return false;
    }
    
    Serial.println("Update complete!");
    sendOTAStatus("UPDATED", 100);
    
    // Yield to watchdog before restart
    bool inTask = xTaskGetCurrentTaskHandle() != NULL;
    if (inTask) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
        delay(1000);
    }
    
    Serial.println("Rebooting...");
    if (inTask) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
        delay(1000);
    }
    
    ESP.restart();
    
    return true;
}

#endif // OTA_MANAGER_H
