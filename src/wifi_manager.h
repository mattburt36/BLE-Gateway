/**
 * WiFi Manager
 * 
 * Handles:
 * - WiFi connection
 * - Access Point mode for configuration
 * - Web server for configuration portal
 * - NTP time synchronization
 * - Remote configuration fetching
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

extern String wifi_ssid;
extern String wifi_password;
extern String mqtt_host;
extern String mqtt_user;
extern String mqtt_password;
extern String device_id;
extern String company;
extern String development;
extern String firmware_url;
extern bool wifi_connected;
extern bool time_synced;
extern unsigned long current_timestamp;
extern WebServer webServer;
extern DNSServer dnsServer;

const char* AP_SSID = "BLE-Gateway-Setup";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// NTP configuration
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET = 0;
const int DAYLIGHT_OFFSET = 0;

// Remote config URL
const char* CONFIG_SERVER = "http://gwconfig.hoptech.co.nz";

bool connectWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", wifi_ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✓ WiFi connected");
        Serial.printf("  IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
        return true;
    } else {
        Serial.println("✗ WiFi connection failed");
        return false;
    }
}

bool syncTimeNTP() {
    Serial.println("Synchronizing time with NTP server...");
    
    configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);
    
    int attempts = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        delay(500);
        attempts++;
    }
    
    if (attempts < 10) {
        current_timestamp = time(nullptr);
        Serial.printf("✓ Time synchronized: %s", asctime(&timeinfo));
        return true;
    } else {
        Serial.println("✗ NTP sync failed");
        return false;
    }
}

bool fetchRemoteConfig() {
    if (!wifi_connected) {
        Serial.println("Cannot fetch remote config - WiFi not connected");
        return false;
    }
    
    HTTPClient http;
    String url = String(CONFIG_SERVER) + "/" + device_id;
    
    Serial.printf("Fetching remote configuration from: %s\n", url.c_str());
    
    http.begin(url);
    http.addHeader("X-Device-ID", device_id);
    http.addHeader("X-Firmware-Version", FIRMWARE_VERSION);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // Parse JSON response
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            // Extract configuration
            if (doc.containsKey("development")) {
                development = doc["development"].as<String>();
            }
            if (doc.containsKey("firmware")) {
                firmware_url = doc["firmware"].as<String>();
            }
            if (doc.containsKey("company")) {
                company = doc["company"].as<String>();
            }
            if (doc.containsKey("mqtt_host")) {
                mqtt_host = doc["mqtt_host"].as<String>();
            }
            if (doc.containsKey("mqtt_user")) {
                mqtt_user = doc["mqtt_user"].as<String>();
            }
            if (doc.containsKey("mqtt_password")) {
                mqtt_password = doc["mqtt_password"].as<String>();
            }
            
            // Use timestamp from config if NTP failed
            if (!time_synced && doc.containsKey("timestamp")) {
                current_timestamp = doc["timestamp"].as<unsigned long>();
                time_synced = true;
                Serial.println("✓ Time synchronized from config server");
            }
            
            Serial.println("✓ Remote configuration retrieved:");
            Serial.printf("  Company: %s\n", company.c_str());
            Serial.printf("  Development: %s\n", development.c_str());
            Serial.printf("  MQTT Host: %s\n", mqtt_host.c_str());
            
            http.end();
            return true;
        } else {
            Serial.printf("✗ JSON parsing failed: %s\n", error.c_str());
        }
    } else {
        Serial.printf("✗ HTTP request failed, code: %d\n", httpCode);
    }
    
    http.end();
    return false;
}

void handleConfigRoot() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>BLE Gateway Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; }
        input { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; }
        button { background: #4CAF50; color: white; padding: 12px; border: none; width: 100%; cursor: pointer; font-size: 16px; }
        button:hover { background: #45a049; }
        .info { background: #e7f3fe; padding: 10px; border-left: 4px solid #2196F3; margin-bottom: 15px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 BLE Gateway Setup</h1>
        <div class="info">
            <strong>Device ID:</strong> )" + device_id + R"(
        </div>
        <form action="/save" method="POST">
            <h3>WiFi Settings</h3>
            <input type="text" name="ssid" placeholder="WiFi SSID" required>
            <input type="password" name="password" placeholder="WiFi Password" required>
            
            <h3>MQTT Settings</h3>
            <input type="text" name="mqtt_host" placeholder="MQTT Host" value="mqtt.hoptech.co.nz">
            <input type="text" name="mqtt_user" placeholder="MQTT Username (optional)">
            <input type="password" name="mqtt_pass" placeholder="MQTT Password (optional)">
            
            <button type="submit">Save & Restart</button>
        </form>
    </div>
</body>
</html>
)";
    
    webServer.send(200, "text/html", html);
}

void handleConfigSave() {
    wifi_ssid = webServer.arg("ssid");
    wifi_password = webServer.arg("password");
    mqtt_host = webServer.arg("mqtt_host");
    mqtt_user = webServer.arg("mqtt_user");
    mqtt_password = webServer.arg("mqtt_pass");
    
    saveConfig();
    
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Configuration Saved</title>
    <meta http-equiv="refresh" content="3;url=/">
    <style>
        body { font-family: Arial; margin: 20px; text-align: center; }
        .success { color: #4CAF50; font-size: 24px; margin: 50px; }
    </style>
</head>
<body>
    <div class="success">
        ✓ Configuration saved!<br>
        Restarting device...
    </div>
</body>
</html>
)";
    
    webServer.send(200, "text/html", html);
    
    delay(2000);
    ESP.restart();
}

void startConfigPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    dnsServer.start(53, "*", AP_IP);
    
    webServer.on("/", handleConfigRoot);
    webServer.on("/save", HTTP_POST, handleConfigSave);
    webServer.onNotFound(handleConfigRoot);
    webServer.begin();
    
    Serial.println("✓ Configuration portal started");
    Serial.printf("  SSID: %s\n", AP_SSID);
    Serial.printf("  Password: %s\n", AP_PASSWORD);
    Serial.printf("  IP: %s\n", AP_IP.toString().c_str());
}

void wifiMonitorTask(void* parameter) {
    Serial.println("WiFi Monitor Task started");
    
    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, attempting reconnection...");
            wifi_connected = false;
            
            if (connectWiFi()) {
                wifi_connected = true;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10 seconds
    }
}

#endif // WIFI_MANAGER_H
