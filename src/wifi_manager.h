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
#include <WiFiClientSecure.h>
#include <time.h>
#include <ArduinoJson.h>

extern String wifi_ssid;
extern String wifi_password;
extern String mqtt_host;
extern String mqtt_user;
extern String mqtt_password;
extern String device_id;
extern String device_token;
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

// NTP configuration - Use Cloudflare NTP with local fallback
const char* NTP_SERVER = "time.cloudflare.com";  // Cloudflare NTP (anycast, fast, secure)
const char* NTP_SERVER_BACKUP = "mqtt.hoptech.co.nz"; // Local fallback
// Timezone configuration
// Note: ESP32 configTime uses seconds offset, not POSIX TZ string for reliability
// Pacific/Auckland: UTC+13 during NZDT (Sept-April), UTC+12 during NZST (April-Sept)
const long GMT_OFFSET_SEC = 13 * 3600;  // Current: NZDT (UTC+13) - manually adjust in April
const int DAYLIGHT_OFFSET_SEC = 0;

bool connectWiFi() {
    Serial.println("\n========== WiFi CONNECTION ATTEMPT ==========");
    Serial.printf("📡 SSID: %s\n", wifi_ssid.c_str());
    Serial.printf("🔑 Password: %s\n", wifi_password.length() > 0 ? "***SET***" : "(none)");
    Serial.printf("📶 Current Status: %d\n", WiFi.status());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    Serial.print("⏳ Connecting");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ ✅ ✅ WiFi CONNECTED! ✅ ✅ ✅");
        Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("   DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("   MAC: %s\n", WiFi.macAddress().c_str());
        Serial.println("==========================================\n");
        return true;
    } else {
        Serial.println("❌ ❌ ❌ WiFi CONNECTION FAILED! ❌ ❌ ❌");
        Serial.printf("   Status Code: %d\n", WiFi.status());
        Serial.println("\n🔧 TROUBLESHOOTING:");
        switch(WiFi.status()) {
            case WL_NO_SSID_AVAIL:
                Serial.println("   → SSID not found. Check:");
                Serial.println("      1. SSID is spelled correctly");
                Serial.println("      2. Router is powered on");
                Serial.println("      3. Device is in range");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("   → Connection failed. Check:");
                Serial.println("      1. Password is correct");
                Serial.println("      2. Security mode (WPA2 recommended)");
                break;
            case WL_DISCONNECTED:
                Serial.println("   → Disconnected/Timeout. Check:");
                Serial.println("      1. Signal strength");
                Serial.println("      2. Router accepting new connections");
                Serial.println("      3. MAC filtering on router");
                break;
        }
        Serial.println("==========================================\n");
        return false;
    }
}

bool syncTimeNTP() {
    Serial.println("Synchronizing time with NTP server...");
    Serial.printf("Primary NTP: %s, Backup: %s\n", NTP_SERVER, NTP_SERVER_BACKUP);
    Serial.printf("Timezone offset: UTC%+ld hours (NZDT)\n", GMT_OFFSET_SEC / 3600);
    
    // Configure NTP with local server and backup
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, NTP_SERVER_BACKUP);
    
    int attempts = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        delay(500);
        attempts++;
    }
    
    if (attempts < 10) {
        current_timestamp = time(nullptr);
        Serial.printf("✓ Time synchronized: %s", asctime(&timeinfo));
        Serial.printf("   Local time server working: %s\n", NTP_SERVER);
        return true;
    } else {
        Serial.println("✗ NTP sync failed from both servers");
        return false;
    }
}

void handleConfigRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>BLE Gateway Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; }";
    html += "input { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; }";
    html += "button { background: #4CAF50; color: white; padding: 12px; border: none; width: 100%; cursor: pointer; font-size: 16px; }";
    html += "button:hover { background: #45a049; }";
    html += ".info { background: #e7f3fe; padding: 10px; border-left: 4px solid #2196F3; margin-bottom: 15px; }";
    html += ".success { background: #d4edda; padding: 10px; border-left: 4px solid #28a745; margin-bottom: 15px; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>BLE Gateway Setup</h1>";
    html += "<div class='info'><strong>Device ID:</strong> " + device_id + "</div>";
    html += "<div class='success'><strong>✓ ThingsBoard Integration</strong><br>";
    html += "MQTT Broker: mqtt.hoptech.co.nz<br>";
    html += "Test credentials: test / hoptech-test</div>";
    html += "<div class='info'><strong>WiFi Configuration</strong><br>";
    html += "Configure your WiFi network credentials to connect to the internet.</div>";
    html += "<form action='/save' method='POST'>";
    html += "<h3>WiFi Settings</h3>";
    html += "<input type='text' name='ssid' placeholder='WiFi SSID' required>";
    html += "<input type='password' name='password' placeholder='WiFi Password' required>";
    html += "<button type='submit'>Save WiFi & Restart</button>";
    html += "</form></div></body></html>";
    
    webServer.send(200, "text/html", html);
}

void handleConfigSave() {
    wifi_ssid = webServer.arg("ssid");
    wifi_password = webServer.arg("password");
    
    saveConfig();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Configuration Saved</title>";
    html += "<meta http-equiv='refresh' content='3;url=/'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; text-align: center; }";
    html += ".success { color: #4CAF50; font-size: 24px; margin: 50px; }";
    html += "</style></head><body>";
    html += "<div class='success'>WiFi configuration saved!<br>Restarting device...</div>";
    html += "</body></html>";
    
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
