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

extern WiFiClient mqttPlainClient;
extern PubSubClient mqttClient;
extern String mqtt_host;
extern String mqtt_user;
extern String mqtt_password;
extern String device_id;
extern bool mqtt_connected;
extern SemaphoreHandle_t mqttMutex;

const int MQTT_PORT = 1883;  // Plain MQTT port (testing)
const int MQTT_KEEPALIVE_SEC = 60;

bool publishConnectMessage() {
    if (!mqtt_connected) {
        return false;
    }
    
    String topic = "sensor/connect";
    
    JsonDocument doc;
    doc["serialNumber"] = device_id;
    doc["sensorType"] = "BLE-Gateway";
    doc["sensorModel"] = "XIAO-ESP32-S3";
    doc["firmware"] = FIRMWARE_VERSION;
    
    String payload;
    serializeJson(doc, payload);
    
    bool success = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = mqttClient.publish(topic.c_str(), payload.c_str());
        xSemaphoreGive(mqttMutex);
    }
    
    if (success) {
        Serial.printf("📤 Published connect message to %s\n", topic.c_str());
    }
    
    return success;
}

bool publishDisconnectMessage() {
    if (!mqttClient.connected()) {
        return false;
    }
    
    String topic = "sensor/disconnect";
    
    JsonDocument doc;
    doc["serialNumber"] = device_id;
    
    String payload;
    serializeJson(doc, payload);
    
    bool success = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = mqttClient.publish(topic.c_str(), payload.c_str());
        xSemaphoreGive(mqttMutex);
    }
    
    return success;
}

const char* getMQTTStateString(int state) {
    switch(state) {
        case -4: return "MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time";
        case -3: return "MQTT_CONNECTION_LOST - the network connection was broken";
        case -2: return "MQTT_CONNECT_FAILED - the network connection failed";
        case -1: return "MQTT_DISCONNECTED - the client is disconnected cleanly";
        case 0: return "MQTT_CONNECTED - the client is connected";
        case 1: return "MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT";
        case 2: return "MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier";
        case 3: return "MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection";
        case 4: return "MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected";
        case 5: return "MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect";
        default: return "UNKNOWN_STATE";
    }
}

bool connectMQTT() {
    Serial.println("\n========== MQTT CONNECTION ATTEMPT ==========");
    Serial.printf("⏱  Timestamp: %lu\n", millis());
    Serial.printf("📡 MQTT Broker: %s:%d\n", mqtt_host.c_str(), MQTT_PORT);
    Serial.printf("🆔 Device ID: %s\n", device_id.c_str());
    Serial.printf("👤 MQTT User: %s\n", mqtt_user.length() > 0 ? mqtt_user.c_str() : "(none - anonymous)");
    Serial.printf("🔑 MQTT Pass: %s\n", mqtt_password.length() > 0 ? "***SET***" : "(none)");
    Serial.printf("📶 WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "DISCONNECTED!");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("📍 Local IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("📊 WiFi RSSI: %d dBm\n", WiFi.RSSI());
    }
    Serial.printf("💾 Free Heap: %d bytes\n", ESP.getFreeHeap());
    
    // DNS resolution test
    Serial.printf("\n🔍 Resolving hostname: %s...\n", mqtt_host.c_str());
    IPAddress resolvedIP;
    if (WiFi.hostByName(mqtt_host.c_str(), resolvedIP)) {
        Serial.printf("✓ DNS resolved to: %s\n", resolvedIP.toString().c_str());
    } else {
        Serial.println("✗ DNS resolution FAILED!");
        Serial.println("   Check: 1) DNS servers 2) Internet connectivity 3) Hostname spelling");
        return false;
    }
    
    // Stop any existing connection
    Serial.println("\n📴 Stopping any existing connection...");
    mqttPlainClient.stop();
    delay(100);
    
    // Configure MQTT client
    Serial.println("\n⚙️  Configuring MQTT client...");
    mqttClient.setServer(mqtt_host.c_str(), MQTT_PORT);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SEC);
    mqttClient.setBufferSize(4096);
    mqttClient.setCallback(mqttCallback);
    Serial.printf("   Keep-alive: %d seconds\n", MQTT_KEEPALIVE_SEC);
    Serial.println("   Buffer size: 4096 bytes");
    
    String clientId = "BLE-Gateway-" + device_id;
    Serial.printf("\n🔌 Attempting connection with Client ID: %s\n", clientId.c_str());
    
    bool connected = false;
    if (mqtt_user.length() > 0 && mqtt_password.length() > 0) {
        Serial.println("   Using authenticated connection...");
        connected = mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
    } else {
        Serial.println("   Using anonymous connection...");
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("\n✅ ✅ ✅ MQTT CONNECTED SUCCESSFULLY! ✅ ✅ ✅");
        Serial.printf("   Client ID: %s\n", clientId.c_str());
        
        // Subscribe to ThingsBoard-compatible control topics
        String cmdTopic = "gateway/" + device_id + "/command";
        String otaTopic = "gateway/" + device_id + "/ota";
        String rpcRequestTopic = "sensor/" + device_id + "/request/+/+";
        // ThingsBoard attribute updates for OTA (matches your config)
        String attrUpdateTopic = "sensor/" + device_id + "/firmwareVersion";
        
        Serial.println("\n📬 Subscribing to topics...");
        bool cmdSub = mqttClient.subscribe(cmdTopic.c_str(), 1);  // QoS 1
        bool otaSub = mqttClient.subscribe(otaTopic.c_str(), 1);  // QoS 1
        bool rpcSub = mqttClient.subscribe(rpcRequestTopic.c_str(), 1);  // QoS 1
        bool attrSub = mqttClient.subscribe(attrUpdateTopic.c_str(), 1);  // QoS 1
        
        Serial.printf("   %s %s (QoS 1)\n", cmdSub ? "[OK]" : "[FAIL]", cmdTopic.c_str());
        Serial.printf("   %s %s (QoS 1)\n", otaSub ? "[OK]" : "[FAIL]", otaTopic.c_str());
        Serial.printf("   %s %s (QoS 1)\n", rpcSub ? "[OK]" : "[FAIL]", rpcRequestTopic.c_str());
        Serial.printf("   %s %s (QoS 1, ThingsBoard OTA)\n", attrSub ? "[OK]" : "[FAIL]", attrUpdateTopic.c_str());
        
        // Publish connect message to ThingsBoard
        publishConnectMessage();
        
        Serial.println("==========================================\n");
        return true;
    } else {
        int state = mqttClient.state();
        Serial.println("\n❌ ❌ ❌ MQTT CONNECTION FAILED! ❌ ❌ ❌");
        Serial.printf("   Error Code: %d\n", state);
        Serial.printf("   Error: %s\n", getMQTTStateString(state));
        
        Serial.println("\n🔧 TROUBLESHOOTING STEPS:");
        switch(state) {
            case -4:
                Serial.println("   → Server not responding. Check:");
                Serial.println("      1. Is the MQTT broker running?");
                Serial.println("      2. Can you ping the server?");
                Serial.println("      3. Is there a firewall blocking port 1883?");
                break;
            case -3:
            case -2:
                Serial.println("   → Network issue. Check:");
                Serial.println("      1. Is WiFi connected? (see status above)");
                Serial.println("      2. Can the device reach the internet?");
                Serial.println("      3. Check DNS resolution");
                break;
            case 1:
                Serial.println("   → Protocol mismatch. Check:");
                Serial.println("      1. Broker MQTT version (should be 3.1.1)");
                Serial.println("      2. Update PubSubClient library if old");
                break;
            case 2:
                Serial.println("   → Client ID rejected. Check:");
                Serial.println("      1. Is another client using the same ID?");
                Serial.println("      2. Does broker allow this client ID format?");
                break;
            case 3:
                Serial.println("   → Server unavailable. Check:");
                Serial.println("      1. Is MQTT service running on the broker?");
                Serial.println("      2. Check broker logs for errors");
                Serial.println("      3. Is broker at capacity?");
                break;
            case 4:
                Serial.println("   → Bad credentials! Check:");
                Serial.println("      1. Username is correct");
                Serial.println("      2. Password is correct");
                Serial.println("      3. User has permission to connect");
                Serial.println("      4. Try fetching config from server again");
                break;
            case 5:
                Serial.println("   → Not authorized. Check:");
                Serial.println("      1. User account is active");
                Serial.println("      2. ACL rules allow this device");
                Serial.println("      3. Device is registered on server");
                break;
        }
        
        Serial.println("==========================================\n");
        return false;
    }
}

bool publishDeviceData(const String& deviceMac, const JsonDocument& data, bool isSensor) {
    if (!mqtt_connected) {
        Serial.println("⚠️  Cannot publish: MQTT not connected");
        return false;
    }
    
    // Publish to sensor/data topic (matches your ThingsBoard connector)
    String topic = "sensor/data";
    
    // Build ThingsBoard-compatible JSON payload
    JsonDocument doc;
    
    // Device identification (used by ThingsBoard to identify the device)
    doc["serialNumber"] = deviceMac;
    // Use the type from data (e.g., "LOP001") for both sensorType and sensorModel
    String deviceType = data["type"].as<String>();
    doc["sensorType"] = deviceType;
    doc["sensorModel"] = deviceType;
    
    // Telemetry data - only include temp/humidity/battery for sensor devices
    if (isSensor) {
        // Use exact field names from your connector config: temp and hum
        doc["temp"] = data["temperature"];
        doc["hum"] = data["humidity"];
        // Only include battery if present in data (LOP001 has no battery)
        if (data["battery"].is<int>() && data["battery"].as<int>() > 0) {
            doc["battery"] = data["battery"];
        }
    }
    // Note: For non-sensor devices, we don't include temp/hum/battery at all
    
    doc["rssi"] = data["rssi"];
    
    // Additional metadata
    doc["gateway"] = device_id;
    doc["timestamp"] = data["timestamp"];
    
    String payload;
    serializeJson(doc, payload);
    
    bool success = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = mqttClient.publish(topic.c_str(), payload.c_str());
        xSemaphoreGive(mqttMutex);
    } else {
        Serial.println("⚠️  Failed to acquire MQTT mutex for publish");
        return false;
    }
    
    if (success) {
        Serial.printf("📤 Published to %s (size: %d bytes)\n", topic.c_str(), payload.length());
        if (isSensor) {
            Serial.printf("   Device: %s, Temp: %.2f°C, Hum: %.2f%%\n", 
                         deviceMac.c_str(), 
                         data["temperature"].as<float>(), 
                         data["humidity"].as<float>());
        } else {
            Serial.printf("   Device: %s (non-sensor, RSSI: %d)\n", 
                         deviceMac.c_str(), 
                         data["rssi"].as<int>());
        }
    } else {
        Serial.printf("❌ Failed to publish to %s\n", topic.c_str());
        Serial.printf("   MQTT state: %d (%s)\n", mqttClient.state(), getMQTTStateString(mqttClient.state()));
        Serial.printf("   Payload size: %d bytes\n", payload.length());
    }
    
    return success;
}

bool publishGatewayStatus() {
    if (!mqtt_connected) {
        Serial.println("⚠️  Cannot publish status: MQTT not connected");
        return false;
    }
    
    // Publish gateway as its own device (not as sensor, just status attributes)
    String topic = "gateway/status";
    
    JsonDocument doc;
    doc["serialNumber"] = device_id;
    doc["sensorType"] = "BLE-Gateway";
    doc["sensorModel"] = "XIAO-ESP32-S3";
    doc["firmware"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wifiRssi"] = WiFi.RSSI();
    
    // Add timestamp in milliseconds
    unsigned long long ts_millis = (unsigned long long)current_timestamp * 1000ULL;
    doc["timestamp"] = ts_millis;
    
    String payload;
    serializeJson(doc, payload);
    
    bool success = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = mqttClient.publish(topic.c_str(), payload.c_str());
        xSemaphoreGive(mqttMutex);
    } else {
        Serial.println("⚠️  Failed to acquire MQTT mutex for status publish");
        return false;
    }
    
    if (success) {
        Serial.printf("📊 Gateway status published (uptime: %lu sec)\n", millis() / 1000);
    } else {
        Serial.printf("❌ Failed to publish gateway status\n");
        Serial.printf("   MQTT state: %d (%s)\n", mqttClient.state(), getMQTTStateString(mqttClient.state()));
    }
    
    return success;
}

void mqttMaintenanceTask(void* parameter) {
    Serial.println("🔄 MQTT Maintenance Task started");
    
    unsigned long lastStatusSend = 0;
    unsigned long lastDebugOutput = 0;
    const unsigned long STATUS_INTERVAL = 300000; // 5 minutes
    const unsigned long DEBUG_INTERVAL = 30000; // 30 seconds
    
    while (true) {
        unsigned long now = millis();
        
        // Periodic debug output
        if (now - lastDebugOutput > DEBUG_INTERVAL) {
            Serial.printf("\n[MQTT Task] Status check - Connected: %s, State: %d (%s)\n", 
                         mqtt_connected ? "YES" : "NO", 
                         mqttClient.state(),
                         getMQTTStateString(mqttClient.state()));
            lastDebugOutput = now;
        }
        
        if (!mqttClient.connected()) {
            mqtt_connected = false;
            Serial.println("\n⚠️  MQTT disconnected, attempting reconnection...");
            Serial.printf("   Last state: %d (%s)\n", mqttClient.state(), getMQTTStateString(mqttClient.state()));
            
            if (connectMQTT()) {
                mqtt_connected = true;
                Serial.println("✅ Reconnection successful!");
            } else {
                Serial.println("❌ Reconnection failed, will retry in 5 seconds...");
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
        if (now - lastStatusSend > STATUS_INTERVAL) {
            Serial.println("\n⏰ Time to send periodic status update...");
            publishGatewayStatus();
            lastStatusSend = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Run every 100ms
    }
    
    // Cleanup on task deletion (publish disconnect message)
    Serial.println("📴 MQTT task ending - publishing disconnect message...");
    publishDisconnectMessage();
}

#endif // MQTT_HANDLER_H
