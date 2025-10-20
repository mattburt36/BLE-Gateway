#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>

// NTP configuration
#define NTP_SERVER "pool.ntp.org"
#define NTP_SERVER_BACKUP "time.nist.gov"

// External references
extern String wifi_ssid;
extern String wifi_password;
extern String thingsboard_host;
extern String thingsboard_token;
extern String config_url;
extern String config_username;
extern String config_password;
extern bool wifi_connected;
extern bool config_mode;
extern bool time_synced;

extern WebServer server;
extern DNSServer dnsServer;

// Forward declarations
extern void saveConfig();

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

// ==== NTP TIME SYNC ====
bool syncTime() {
    Serial.println("Synchronizing time with NTP server...");
    configTime(0, 0, NTP_SERVER, NTP_SERVER_BACKUP);
    
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts++ < 20) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year > (2020 - 1900)) {
            time_synced = true;
            Serial.println("Time synchronized successfully!");
            Serial.printf("Current time: %s", asctime(&timeinfo));
            return true;
        }
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nTime sync failed, but continuing...");
    return false;
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
        html += "<style>body{font-family:Arial;max-width:600px;margin:20px auto;padding:20px;}";
        html += "input,button{width:100%;padding:10px;margin:8px 0;box-sizing:border-box;}";
        html += "button{background:#4CAF50;color:white;border:none;cursor:pointer;font-size:16px;}";
        html += "button:hover{background:#45a049;}";
        html += "h3{margin-top:20px;color:#333;border-bottom:2px solid #4CAF50;padding-bottom:5px;}";
        html += "label{font-weight:bold;display:block;margin-top:10px;}";
        html += ".info{background:#f0f0f0;padding:10px;margin:10px 0;border-radius:5px;}</style></head><body>";
        html += "<h2>🔧 BLE Gateway Configuration</h2>";
        html += "<div class='info'><strong>Firmware:</strong> " + String(FIRMWARE_TITLE) + " v" + String(FIRMWARE_VERSION) + "<br>";
        html += "<strong>MAC:</strong> " + WiFi.macAddress() + "</div>";
        html += "<form action='/save' method='post'>";
        
        html += "<h3>WiFi Settings</h3>";
        html += "<label>WiFi SSID:</label><input type='text' name='ssid' value='" + wifi_ssid + "' required>";
        html += "<label>WiFi Password:</label><input type='password' name='pass' value='' placeholder='Enter password'>";
        
        html += "<h3>MQTT Settings</h3>";
        html += "<label>ThingsBoard MQTT Host:</label><input type='text' name='tbhost' value='" + thingsboard_host + "' placeholder='mqtt.thingsboard.cloud' required>";
        html += "<label>Device Access Token:</label><input type='text' name='tbtoken' value='' placeholder='Enter device token' required>";
        
        html += "<h3>Config Fallback (Optional)</h3>";
        html += "<label>Config URL:</label><input type='text' name='cfgurl' value='" + config_url + "' placeholder='https://hoptech.co.nz/bgw-config/'>";
        html += "<label>Config Username:</label><input type='text' name='cfguser' value='" + config_username + "' placeholder='Optional'>";
        html += "<label>Config Password:</label><input type='password' name='cfgpass' value='' placeholder='Optional'>";
        
        html += "<button type='submit'>💾 Save and Restart</button></form>";
        html += "<div class='info' style='margin-top:20px;'>Note: Passwords are encrypted before storage</div>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, []() {
        wifi_ssid = server.arg("ssid");
        if (server.arg("pass").length() > 0) {
            wifi_password = server.arg("pass");
        }
        thingsboard_host = server.arg("tbhost");
        if (server.arg("tbtoken").length() > 0) {
            thingsboard_token = server.arg("tbtoken");
        }
        
        // Optional config fallback settings
        if (server.arg("cfgurl").length() > 0) {
            config_url = server.arg("cfgurl");
        }
        if (server.arg("cfguser").length() > 0) {
            config_username = server.arg("cfguser");
        }
        if (server.arg("cfgpass").length() > 0) {
            config_password = server.arg("cfgpass");
        }
        
        saveConfig();
        server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='10;url=/'/></head><body style='font-family:Arial;text-align:center;padding:50px;'><h2>✅ Configuration Saved!</h2><p>Device restarting...</p><p>You will be redirected in 10 seconds.</p></body></html>");
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
    if (wifi_connected) {
        Serial.println("\nWiFi connected!");
        printWiFiStatus();
        
        // Try to sync time once WiFi is connected
        syncTime();
    } else {
        Serial.println("\nWiFi failed.");
    }
    return wifi_connected;
}

#endif // WIFI_MANAGER_H
