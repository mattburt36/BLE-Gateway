#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <map>
#include <time.h>
#include "ota_manager.h"
#include "ble_scanner.h"

// External references
extern String thingsboard_host;
extern String thingsboard_token;
extern String config_url;
extern WiFiClient espClient;
extern PubSubClient client;
extern bool mqtt_connected;
extern bool wifi_connected;
extern bool time_synced;
extern bool config_mode;
extern std::map<String, BLEAdvertEntry> detectionBuffer;
extern SemaphoreHandle_t detectionBufferMutex;
extern SemaphoreHandle_t mqttMutex;
extern OTAState otaState;
extern unsigned long mqttFailStart;
extern bool apModeOffered;

// Forward declarations
extern bool fetchConfigFromURL();
extern String toHex(const String& data);
extern bool performOTAUpdate();

const unsigned long MQTT_FAIL_AP_TIMEOUT = 300000;

// ==== OTA STATUS REPORTING ====
void sendOTAStatus(const String& status, int progress = -1) {
    if (!mqtt_connected) return;
    
    JsonDocument doc;
    doc["current_fw_title"] = FIRMWARE_TITLE;
    doc["current_fw_version"] = FIRMWARE_VERSION;
    doc["fw_state"] = status;
    
    if (progress >= 0) {
        doc["fw_progress"] = progress;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    Serial.printf("Sending OTA status: %s (%d%%)\n", status.c_str(), progress);
    client.publish("v1/devices/me/attributes", payload.c_str());
}

// ==== GATEWAY ATTRIBUTES ====
void sendGatewayAttributes() {
    if (!mqtt_connected) return;
    
    JsonDocument doc;
    
    // Firmware information
    doc["current_fw_title"] = FIRMWARE_TITLE;
    doc["current_fw_version"] = FIRMWARE_VERSION;
    doc["fw_state"] = otaState.updateInProgress ? otaState.status : "IDLE";
    
    // Hardware information
    doc["chipModel"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();
    doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["sdkVersion"] = ESP.getSdkVersion();
    
    // Temperature (internal chip temperature)
    doc["chipTemperature"] = temperatureRead();
    
    // Network information
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["macAddress"] = WiFi.macAddress();
    doc["rssi"] = WiFi.RSSI();
    
    // Time sync status
    doc["timeSynced"] = time_synced;
    if (time_synced) {
        time_t now;
        time(&now);
        doc["timestamp"] = now;
    }
    
    // OTA partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        doc["otaPartition"] = running->label;
        doc["otaPartitionSize"] = running->size;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    Serial.println("Sending gateway attributes:");
    Serial.println(payload);
    
    client.publish("v1/devices/me/attributes", payload.c_str());
}

// ==== MQTT CALLBACK ====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("\n========== MQTT Message Received ==========\n");
    Serial.printf("Topic: %s\n", topic);
    
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("Payload: %s\n", message.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return;
    }
    
    String topicStr = String(topic);
    
    // Handle shared attributes updates (firmware updates from ThingsBoard)
    if (topicStr == "v1/devices/me/attributes" || topicStr.indexOf("/attributes") >= 0) {
        JsonObject shared = doc["shared"].is<JsonObject>() ? doc["shared"].as<JsonObject>() : doc.as<JsonObject>();
        
        // Check for firmware update attributes
        if (shared["fw_title"].is<String>() || shared["fw_version"].is<String>() || 
            shared["fw_url"].is<String>() || shared["target_fw_version"].is<String>()) {
            
            otaState.firmwareTitle = shared["fw_title"] | FIRMWARE_TITLE;
            otaState.firmwareVersion = shared["fw_version"] | shared["target_fw_version"] | "";
            otaState.firmwareUrl = shared["fw_url"] | "";
            otaState.firmwareSize = shared["fw_size"] | 0;
            otaState.firmwareChecksum = shared["fw_checksum"] | "";
            
            Serial.println("\n>>> FIRMWARE UPDATE REQUEST <<<");
            Serial.printf("Firmware: %s v%s\n", otaState.firmwareTitle.c_str(), otaState.firmwareVersion.c_str());
            Serial.printf("URL: %s\n", otaState.firmwareUrl.c_str());
            Serial.printf("Current Version: %s\n", FIRMWARE_VERSION);
            
            // Check if update is needed
            if (otaState.firmwareVersion != FIRMWARE_VERSION && otaState.firmwareUrl.length() > 0) {
                otaState.updateAvailable = true;
                Serial.println("New firmware version available!");
                sendOTAStatus("DOWNLOADING", 0);
            } else {
                Serial.println("Already on latest firmware version");
                sendOTAStatus("UP_TO_DATE", 100);
            }
        }
    }
    
    // Handle RPC requests
    if (topicStr.indexOf("/rpc/request/") >= 0) {
        if (doc["method"].is<String>()) {
            String method = doc["method"].as<String>();
            
            // Extract request ID for response
            String requestId = "";
            int reqIdx = topicStr.lastIndexOf("/");
            if (reqIdx >= 0) {
                requestId = topicStr.substring(reqIdx + 1);
            }
            
            if (method == "updateFirmware") {
                JsonObject params = doc["params"];
                
                otaState.firmwareTitle = params["fw_title"] | FIRMWARE_TITLE;
                otaState.firmwareVersion = params["fw_version"] | "";
                otaState.firmwareUrl = params["fw_url"] | "";
                otaState.firmwareSize = params["fw_size"] | 0;
                otaState.firmwareChecksum = params["fw_checksum"] | "";
                
                Serial.println("\n>>> RPC FIRMWARE UPDATE REQUEST <<<");
                Serial.printf("Firmware: %s v%s\n", otaState.firmwareTitle.c_str(), otaState.firmwareVersion.c_str());
                
                otaState.updateAvailable = true;
                
                // Send RPC response
                if (requestId.length() > 0) {
                    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
                    JsonDocument responseDoc;
                    responseDoc["status"] = "accepted";
                    responseDoc["current_version"] = FIRMWARE_VERSION;
                    responseDoc["target_version"] = otaState.firmwareVersion;
                    
                    String response;
                    serializeJson(responseDoc, response);
                    client.publish(responseTopic.c_str(), response.c_str());
                }
            }
            else if (method == "getCurrentFirmware") {
                // Send RPC response with current firmware info
                if (requestId.length() > 0) {
                    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
                    JsonDocument responseDoc;
                    responseDoc["fw_title"] = FIRMWARE_TITLE;
                    responseDoc["fw_version"] = FIRMWARE_VERSION;
                    responseDoc["fw_state"] = otaState.updateInProgress ? otaState.status : "IDLE";
                    
                    String response;
                    serializeJson(responseDoc, response);
                    client.publish(responseTopic.c_str(), response.c_str());
                }
            }
            else if (method == "reboot") {
                // Send RPC response
                if (requestId.length() > 0) {
                    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
                    client.publish(responseTopic.c_str(), "{\"status\":\"rebooting\"}");
                }
                
                delay(1000);
                ESP.restart();
            }
        }
    }
    
    Serial.println("==========================================\n");
}

bool tryMQTT() {
    if (client.connected()) client.disconnect();
    espClient.stop();
    delay(100);
    client.setServer(thingsboard_host.c_str(), 1883);
    client.setCallback(mqttCallback);
    client.setKeepAlive(60);
    client.setSocketTimeout(60);
    
    int attempts = 0;
    while (!client.connected() && attempts++ < 3) {
        Serial.printf("Connecting to MQTT (attempt %d/3)...", attempts);
        String clientId = "BLEGateway-" + WiFi.macAddress();
        clientId.replace(":", "");
        
        if (client.connect(clientId.c_str(), thingsboard_token.c_str(), "")) {
            Serial.println("connected!");
            mqtt_connected = true;
            
            // Subscribe to attribute updates and RPC requests
            client.subscribe("v1/devices/me/attributes");
            client.subscribe("v1/devices/me/attributes/response/+");
            client.subscribe("v1/devices/me/rpc/request/+");
            
            Serial.println("Subscribed to:");
            Serial.println("  - v1/devices/me/attributes");
            Serial.println("  - v1/devices/me/attributes/response/+");
            Serial.println("  - v1/devices/me/rpc/request/+");
            
            // Request shared attributes (to get any pending firmware updates)
            client.publish("v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"fw_title,fw_version,fw_url,fw_size,fw_checksum,target_fw_version\"}");
            
            // Send gateway attributes
            delay(500);
            sendGatewayAttributes();
            
            return true;
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 2 seconds");
            delay(2000);
        }
    }
    
    // If MQTT connection failed, try fetching config from URL
    if (attempts >= 3 && config_url.length() > 0) {
        Serial.println("\nMQTT connection failed, trying config URL fallback...");
        if (fetchConfigFromURL()) {
            // Retry MQTT with potentially updated credentials
            Serial.println("Retrying MQTT with updated config...");
            delay(1000);
            return tryMQTT(); // Recursive call with updated config
        }
    }
    
    mqtt_connected = false;
    return false;
}

void sendBatchToThingsBoardGateway() {
    if (detectionBuffer.empty() || !mqtt_connected) return;

    // Step 1: Connect devices with types
    JsonDocument connectDoc;
    JsonArray devices = connectDoc["device"].to<JsonArray>();
    
    for (const auto& kv : detectionBuffer) {
        JsonObject device = devices.add<JsonObject>();
        device["name"] = kv.first;
        
        if (kv.second.hasTempHumidity) {
            if (kv.second.deviceType == "Hoptech/L02S-EA01") {
                device["type"] = "L02S";
            } else if (kv.second.deviceType == "MOKO-3-axis") {
                device["type"] = "MOKO_3AXIS";
            } else if (kv.second.deviceType == "MOKO-TH") {
                device["type"] = "MOKO_TH";
            } else if (kv.second.deviceType == "MOKO-3-axis-TH") {
                device["type"] = "MOKO_3AXIS_TH";
            } else {
                device["type"] = "BLE_SENSOR";
            }
        } else {
            device["type"] = "BLE_BEACON";
        }
    }
    
    String connectPayload;
    serializeJson(connectDoc, connectPayload);
    
    Serial.println("\n========== Connecting Devices ==========");
    Serial.println(connectPayload);
    bool connectOk = client.publish("v1/gateway/connect", connectPayload.c_str());
    Serial.printf("Device connect: %s\n", connectOk ? "✓ OK" : "✗ FAILED");
    
    delay(100);

    // Step 2: Send attributes
    JsonDocument attrDoc;
    
    for (const auto& kv : detectionBuffer) {
        JsonObject deviceAttrs = attrDoc[kv.first.c_str()].to<JsonObject>();
        deviceAttrs["macAddress"] = kv.first;
        
        if (kv.second.name.length() > 0) {
            deviceAttrs["deviceName"] = kv.second.name;
        }
        
        if (kv.second.hasTempHumidity) {
            deviceAttrs["sensorType"] = kv.second.deviceType;
            deviceAttrs["hasTemperature"] = true;
            deviceAttrs["hasHumidity"] = true;
            deviceAttrs["hasBattery"] = true;
        }
    }
    
    String attrPayload;
    serializeJson(attrDoc, attrPayload);
    
    Serial.println("\n========== Sending Attributes ==========");
    Serial.println(attrPayload);
    bool attrOk = client.publish("v1/gateway/attributes", attrPayload.c_str());
    Serial.printf("Attributes: %s\n", attrOk ? "✓ OK" : "✗ FAILED");
    
    delay(100);

    // Step 3: Send telemetry
    JsonDocument telemetryDoc;
    
    for (const auto& kv : detectionBuffer) {
        JsonArray arr = telemetryDoc[kv.first.c_str()].to<JsonArray>();
        JsonObject entry = arr.add<JsonObject>();
        
        entry["ts"] = kv.second.ts;
        
        JsonObject values = entry["values"].to<JsonObject>();
        values["rssi"] = kv.second.rssi;
        
        if (kv.second.name.length() > 0) {
            values["name"] = kv.second.name;
        }
        
        if (kv.second.mfgData.length() > 0) {
            values["manufacturerData"] = toHex(kv.second.mfgData);
        }
        
        if (kv.second.svcData.length() > 0) {
            values["serviceData"] = toHex(kv.second.svcData);
        }
        
        if (kv.second.hasTempHumidity) {
            values["temperature"] = kv.second.temperature;
            values["humidity"] = kv.second.humidity;
            values["battery_mv"] = kv.second.battery_mv;
        }
    }
    
    String telemetryPayload;
    serializeJson(telemetryDoc, telemetryPayload);
    
    Serial.println("\n========== Sending Telemetry ==========");
    Serial.printf("Devices in batch: %d\n", detectionBuffer.size());
    Serial.printf("Payload length: %d bytes\n", telemetryPayload.length());
    
    bool ok = client.publish("v1/gateway/telemetry", telemetryPayload.c_str());
    
    if (ok) {
        Serial.println("✓ Telemetry published successfully");
    } else {
        Serial.println("✗ Telemetry publish failed!");
        Serial.printf("Client state: %d\n", client.state());
    }
    Serial.println("==========================================\n");
    
    detectionBuffer.clear();
}

// ==== FREERTOS TASKS ====

// Task 1: MQTT maintenance - handles keepalives and reconnection
void mqttMaintenanceTask(void* parameter) {
    Serial.println("MQTT Maintenance Task started");
    unsigned long lastReconnect = 0;
    unsigned long lastAttrSend = 0;
    
    for(;;) {
        if (config_mode) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        
        // Check WiFi connection
        if (!wifi_connected || WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi lost in MQTT task");
            wifi_connected = false;
            mqtt_connected = false;
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        
        // Handle MQTT connection
        if (!client.connected()) {
            mqtt_connected = false;
            unsigned long now = millis();
            
            // Try to reconnect every 30 seconds
            if (now - lastReconnect > 30000) {
                Serial.println("MQTT disconnected, attempting reconnect...");
                if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    if (tryMQTT()) {
                        Serial.println("MQTT reconnected successfully!");
                        mqttFailStart = 0;
                    } else {
                        Serial.println("MQTT reconnection failed");
                        if (mqttFailStart == 0) {
                            mqttFailStart = now;
                        }
                    }
                    xSemaphoreGive(mqttMutex);
                }
                lastReconnect = now;
            }
        } else {
            mqtt_connected = true;
            
            // Call client.loop() to process incoming messages and maintain connection
            if (xSemaphoreTake(mqttMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
                client.loop();
                xSemaphoreGive(mqttMutex);
            }
            
            // Periodically send gateway attributes (every 5 minutes)
            if (millis() - lastAttrSend > 300000) {
                if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    sendGatewayAttributes();
                    xSemaphoreGive(mqttMutex);
                }
                lastAttrSend = millis();
            }
            
            // Check for OTA updates
            if (otaState.updateAvailable && !otaState.updateInProgress) {
                Serial.println("\n>>> Starting OTA Update from MQTT task <<<");
                otaState.updateAvailable = false;
                if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    performOTAUpdate();
                    xSemaphoreGive(mqttMutex);
                }
            }
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS); // Run every 100ms
    }
}

// Task 3: Message processing - handles batch sending to MQTT
void messageProcessingTask(void* parameter) {
    Serial.println("Message Processing Task started");
    
    const unsigned long BATCH_INTERVAL = 60000;
    
    for(;;) {
        if (config_mode || otaState.updateInProgress) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        
        // Send batch data every 60 seconds
        vTaskDelay(BATCH_INTERVAL / portTICK_PERIOD_MS);
        
        if (mqtt_connected && !detectionBuffer.empty()) {
            Serial.println("Processing and sending batch data...");
            
            // Take mutex to safely access detection buffer
            if (xSemaphoreTake(detectionBufferMutex, portMAX_DELAY) == pdTRUE) {
                if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    sendBatchToThingsBoardGateway();
                    xSemaphoreGive(mqttMutex);
                }
                xSemaphoreGive(detectionBufferMutex);
            }
        }
    }
}

// Helper functions for OTA manager
void mqttClientLoop() {
    client.loop();
}

void mqttClientPublish(const char* topic, const char* payload) {
    client.publish(topic, payload);
}

#endif // MQTT_HANDLER_H
