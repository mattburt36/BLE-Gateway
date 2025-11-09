/**
 * MQTT Handler
 * 
 * Handles:
 * - MQTTS (TLS) connection and reconnection
 * - Message publishing
 * - Subscription handling
 * - Keepalive maintenance
 * - OTA update notifications
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

extern WiFiClientSecure espClient;
extern PubSubClient mqttClient;
extern String mqtt_host;
extern String mqtt_user;
extern String mqtt_password;
extern String device_id;
extern bool mqtt_connected;
extern SemaphoreHandle_t mqttMutex;

const int MQTT_PORT = 8883;  // MQTTS port
const int MQTT_KEEPALIVE = 60;

bool connectMQTT() {
    Serial.printf("Connecting to MQTTS: %s:%d\n", mqtt_host.c_str(), MQTT_PORT);
    
    // Configure secure client
    espClient.setInsecure();  // Accept any certificate (or load CA cert for verification)
    
    mqttClient.setServer(mqtt_host.c_str(), MQTT_PORT);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE);
    mqttClient.setBufferSize(4096);
    mqttClient.setCallback(mqttCallback);  // Set callback for incoming messages
    
    String clientId = "BLE-Gateway-" + device_id;
    
    bool connected = false;
    if (mqtt_user.length() > 0 && mqtt_password.length() > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("✓ MQTTS connected");
        Serial.printf("  Client ID: %s\n", clientId.c_str());
        
        // Subscribe to control topics
        String cmdTopic = "gateway/" + device_id + "/command";
        String otaTopic = "gateway/" + device_id + "/ota";
        mqttClient.subscribe(cmdTopic.c_str());
        mqttClient.subscribe(otaTopic.c_str());
        Serial.printf("  Subscribed to: %s\n", cmdTopic.c_str());
        Serial.printf("  Subscribed to: %s\n", otaTopic.c_str());
        
        return true;
    } else {
        Serial.printf("✗ MQTTS connection failed, state: %d\n", mqttClient.state());
        return false;
    }
}

bool publishDeviceData(const String& deviceMac, const JsonDocument& data) {
    if (!mqtt_connected) {
        return false;
    }
    
    String topic = "gateway/" + device_id + "/device/" + deviceMac;
    
    String payload;
    serializeJson(data, payload);
    
    bool success = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = mqttClient.publish(topic.c_str(), payload.c_str(), false);
        xSemaphoreGive(mqttMutex);
    }
    
    if (success) {
        Serial.printf("Published to %s\n", topic.c_str());
    } else {
        Serial.printf("Failed to publish to %s\n", topic.c_str());
    }
    
    return success;
}

bool publishGatewayStatus() {
    if (!mqtt_connected) {
        return false;
    }
    
    String topic = "gateway/" + device_id + "/status";
    
    JsonDocument doc;
    doc["device_id"] = device_id;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["timestamp"] = current_timestamp;
    
    String payload;
    serializeJson(doc, payload);
    
    bool success = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = mqttClient.publish(topic.c_str(), payload.c_str(), false);
        xSemaphoreGive(mqttMutex);
    }
    
    return success;
}

void mqttMaintenanceTask(void* parameter) {
    Serial.println("MQTT Maintenance Task started");
    
    unsigned long lastStatusSend = 0;
    const unsigned long STATUS_INTERVAL = 300000; // 5 minutes
    
    while (true) {
        if (!mqttClient.connected()) {
            mqtt_connected = false;
            Serial.println("MQTT disconnected, reconnecting...");
            
            if (connectMQTT()) {
                mqtt_connected = true;
            } else {
                vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5s before retry
                continue;
            }
        }
        
        // Process MQTT messages
        if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            mqttClient.loop();
            xSemaphoreGive(mqttMutex);
        }
        
        // Send gateway status periodically
        if (millis() - lastStatusSend > STATUS_INTERVAL) {
            publishGatewayStatus();
            lastStatusSend = millis();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Run every 100ms
    }
}

#endif // MQTT_HANDLER_H
