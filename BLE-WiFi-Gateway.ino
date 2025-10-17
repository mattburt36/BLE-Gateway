#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <map>

#define EEPROM_SIZE 1024
#define WIFI_SSID_ADDR      0
#define WIFI_PASS_ADDR      64
#define TB_HOST_ADDR        128
#define TB_TOKEN_ADDR       192
#define CONFIG_VALID_ADDR   256

// Firmware version - update this with each release
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_TITLE "BLE-Gateway"

String wifi_ssid = "";
String wifi_password = "";
String thingsboard_host = "";
String thingsboard_token = "";
bool wifi_connected = false;
bool mqtt_connected = false;
bool config_mode = false;

// BLE
BLEScan* pBLEScan;
const int SCAN_TIME = 5;
unsigned long last_ble_scan = 0;

// OTA State
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
OTAState otaState;

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
std::map<String, BLEAdvertEntry> detectionBuffer;
unsigned long lastBatchSend = 0;
const unsigned long BATCH_INTERVAL = 60000;

unsigned long mqttFailStart = 0;
const unsigned long MQTT_FAIL_AP_TIMEOUT = 300000;
bool apModeOffered = false;

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
DNSServer dnsServer;

// Forward declarations
void sendGatewayAttributes();
void sendOTAStatus(const String& status, int progress = -1);

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

// ==== WiFi Status Print ====
void printWiFiStatus() {
    Serial.println("==== WiFi Status ====");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("WiFi Status: ");
    switch (WiFi.status()) {
        case WL_CONNECTED:    Serial.println("Connected"); break;
        case WL_NO_SSID_AVAIL:Serial.println("No SSID Available"); break;
        case WL_CONNECT_FAILED:Serial.println("Connect Failed"); break;
        case WL_IDLE_STATUS:  Serial.println("Idle"); break;
        case WL_DISCONNECTED: Serial.println("Disconnected"); break;
        default:              Serial.println("Unknown"); break;
    }
    Serial.println("=====================");
}

// ==== EEPROM CONFIG ====
void saveConfig() {
    EEPROM.writeString(WIFI_SSID_ADDR, wifi_ssid);
    EEPROM.writeString(WIFI_PASS_ADDR, wifi_password);
    EEPROM.writeString(TB_HOST_ADDR, thingsboard_host);
    EEPROM.writeString(TB_TOKEN_ADDR, thingsboard_token);
    EEPROM.writeByte(CONFIG_VALID_ADDR, 0xAA);
    EEPROM.commit();
    Serial.println("Configuration saved to EEPROM");
}

bool loadConfig() {
    if (EEPROM.readByte(CONFIG_VALID_ADDR) != 0xAA) return false;
    wifi_ssid = EEPROM.readString(WIFI_SSID_ADDR);
    wifi_password = EEPROM.readString(WIFI_PASS_ADDR);
    thingsboard_host = EEPROM.readString(TB_HOST_ADDR);
    thingsboard_token = EEPROM.readString(TB_TOKEN_ADDR);
    return (wifi_ssid.length() && thingsboard_host.length() && thingsboard_token.length());
}

// ==== CONFIG PORTAL ====
void setupAP() {
    config_mode = true;
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("BLE-Gateway-Setup", "12345678");
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void setupWebServer() {
    server.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head><title>BLE Gateway Setup</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>body{font-family:Arial;}input{width:100%;padding:10px;margin:8px 0;}</style></head><body>";
        html += "<h2>BLE Gateway Configuration</h2>";
        html += "<p><strong>Firmware:</strong> " + String(FIRMWARE_TITLE) + " v" + String(FIRMWARE_VERSION) + "</p>";
        html += "<form action='/save' method='post'>";
        html += "<label>WiFi SSID:</label><input type='text' name='ssid' value='" + wifi_ssid + "' required>";
        html += "<label>WiFi Password:</label><input type='password' name='pass' value='" + wifi_password + "'>";
        html += "<label>ThingsBoard MQTT Host (e.g. mqtt.thingsboard.cloud):</label><input type='text' name='tbhost' value='" + thingsboard_host + "' required>";
        html += "<label>Device Token:</label><input type='text' name='tbtoken' value='" + thingsboard_token + "' required>";
        html += "<button type='submit'>Save and Restart</button></form>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, []() {
        wifi_ssid = server.arg("ssid");
        wifi_password = server.arg("pass");
        thingsboard_host = server.arg("tbhost");
        thingsboard_token = server.arg("tbtoken");
        saveConfig();
        server.send(200, "text/html", "<html><body><h2>Saved! Restarting...</h2></body></html>");
        delay(1500);
        ESP.restart();
    });

    server.begin();
    Serial.println("Web server started for config mode");
}

bool tryWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
        delay(500);
        Serial.print(".");
    }
    wifi_connected = (WiFi.status() == WL_CONNECTED);
    if (wifi_connected) Serial.println("\nWiFi connected!");
    else Serial.println("\nWiFi failed.");
    printWiFiStatus();
    return wifi_connected;
}

// ==== OTA STATUS REPORTING ====
void sendOTAStatus(const String& status, int progress) {
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
    
    // Network information
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["macAddress"] = WiFi.macAddress();
    doc["rssi"] = WiFi.RSSI();
    
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
                client.loop(); // Keep MQTT alive
            }
        }
        
        delay(1);
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
    
    delay(1000);
    
    Serial.println("Rebooting...");
    delay(1000);
    
    ESP.restart();
    
    return true;
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
        JsonObject shared = doc.containsKey("shared") ? doc["shared"].as<JsonObject>() : doc.as<JsonObject>();
        
        // Check for firmware update attributes
        if (shared.containsKey("fw_title") || shared.containsKey("fw_version") || 
            shared.containsKey("fw_url") || shared.containsKey("target_fw_version")) {
            
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
        if (doc.containsKey("method")) {
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
    while (!client.connected() && attempts++ < 5) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("BLEGateway", thingsboard_token.c_str(), "")) {
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
    mqtt_connected = false;
    return false;
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
    
    if (detectionBuffer.find(mac) != detectionBuffer.end()) {
        if (detectionBuffer[mac].dataHash == currentHash) {
            detectionBuffer[mac].rssi = rssi;
            detectionBuffer[mac].ts = millis();
            Serial.printf("Duplicate data for %s (RSSI updated to %d dBm)\n", mac.c_str(), rssi);
            return;
        } else {
            Serial.printf("*** Data changed for %s ***\n", mac.c_str());
        }
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

    detectionBuffer[mac] = entry;
    Serial.printf("Queued: %s RSSI:%d Name:%s\n", mac.c_str(), rssi, name.c_str());
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        processAdvert(advertisedDevice);
    }
};

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

// ==== SETUP & LOOP ====
void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.printf("BLE Gateway Starting...\n");
    Serial.printf("Firmware: %s v%s\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
    Serial.println("========================================\n");
    
    EEPROM.begin(EEPROM_SIZE);

    // Initialize OTA state
    otaState.updateAvailable = false;
    otaState.updateInProgress = false;
    otaState.progressPercent = 0;
    otaState.status = "IDLE";

    bool configOk = loadConfig();
    if (configOk) {
        Serial.println("Config loaded, trying WiFi and MQTT...");
        if (tryWiFi() && tryMQTT()) {
            config_mode = false;
            Serial.println("WiFi & MQTT OK, starting BLE scan...");
        } else {
            Serial.println("WiFi or MQTT failed, entering config mode...");
            setupAP();
            setupWebServer();
        }
    } else {
        Serial.println("No valid config, entering config mode...");
        setupAP();
        setupWebServer();
    }

    client.setBufferSize(8192);

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    Serial.println("Setup complete.");
}

void loop() {
    if (config_mode) {
        server.handleClient();
        dnsServer.processNextRequest();
        delay(10);
        return;
    }

    if (!wifi_connected || WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, entering config mode...");
        setupAP();
        setupWebServer();
        return;
    }

    if (!client.connected()) {
        if (mqttFailStart == 0) {
            mqttFailStart = millis();
        }
        Serial.println("MQTT lost, attempting reconnect...");
        mqtt_connected = tryMQTT();
        if (mqtt_connected) {
            mqttFailStart = 0;
            apModeOffered = false;
        } else if (!apModeOffered && (millis() - mqttFailStart > MQTT_FAIL_AP_TIMEOUT)) {
            Serial.println("MQTT failed for over 5 minutes, offering AP config mode...");
            setupAP();
            setupWebServer();
            apModeOffered = true;
            return;
        }
    } else {
        mqttFailStart = 0;
        apModeOffered = false;
    }
    client.loop();

    // Check if OTA update is available and start it
    if (otaState.updateAvailable && !otaState.updateInProgress) {
        Serial.println("\n>>> Starting OTA Update <<<");
        otaState.updateAvailable = false;
        performOTAUpdate();
    }

    unsigned long now = millis();
    
    // Don't scan during OTA update
    if (now - last_ble_scan > 10000 && !otaState.updateInProgress) {
        Serial.println("Scanning BLE...");
        pBLEScan->start(SCAN_TIME, false);
        last_ble_scan = now;
    }

    if (now - lastBatchSend > BATCH_INTERVAL) {
        sendBatchToThingsBoardGateway();
        lastBatchSend = now;
    }

    // Periodically send gateway attributes (every 5 minutes)
    static unsigned long lastAttrSend = 0;
    if (millis() - lastAttrSend > 300000) {
        sendGatewayAttributes();
        lastAttrSend = millis();
    }

    delay(10);
}