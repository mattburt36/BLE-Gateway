#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// EEPROM addresses
#define EEPROM_SIZE 1024
#define WIFI_SSID_ADDR      0
#define WIFI_PASS_ADDR      64
#define TB_HOST_ADDR        128
#define TB_TOKEN_ADDR       192
#define CONFIG_VALID_ADDR   256
#define CONFIG_URL_ADDR     320
#define CONFIG_USER_ADDR    448
#define CONFIG_PASS_ADDR    512

// Configuration URLs
#define CONFIG_FALLBACK_URL "https://hoptech.co.nz/bgw-config/"

// Global configuration variables
extern String wifi_ssid;
extern String wifi_password;
extern String thingsboard_host;
extern String thingsboard_token;
extern String config_url;
extern String config_username;
extern String config_password;

// ==== ENCRYPTION ====
// Simple XOR encryption for stored credentials (basic obfuscation)
String encryptString(const String& data) {
    String encrypted = "";
    const char key[] = "BLE-GW-2025-KEY"; // Simple key, can be improved
    for (size_t i = 0; i < data.length(); i++) {
        encrypted += (char)(data[i] ^ key[i % (sizeof(key) - 1)]);
    }
    return encrypted;
}

String decryptString(const String& data) {
    return encryptString(data); // XOR is symmetric
}

// ==== EEPROM CONFIG ====
void saveConfig() {
    EEPROM.writeString(WIFI_SSID_ADDR, wifi_ssid);
    EEPROM.writeString(WIFI_PASS_ADDR, encryptString(wifi_password));
    EEPROM.writeString(TB_HOST_ADDR, thingsboard_host);
    EEPROM.writeString(TB_TOKEN_ADDR, encryptString(thingsboard_token));
    EEPROM.writeString(CONFIG_URL_ADDR, config_url);
    EEPROM.writeString(CONFIG_USER_ADDR, config_username);
    EEPROM.writeString(CONFIG_PASS_ADDR, encryptString(config_password));
    EEPROM.writeByte(CONFIG_VALID_ADDR, 0xAA);
    EEPROM.commit();
    Serial.println("Configuration saved to EEPROM (credentials encrypted)");
}

bool loadConfig() {
    if (EEPROM.readByte(CONFIG_VALID_ADDR) != 0xAA) return false;
    wifi_ssid = EEPROM.readString(WIFI_SSID_ADDR);
    wifi_password = decryptString(EEPROM.readString(WIFI_PASS_ADDR));
    thingsboard_host = EEPROM.readString(TB_HOST_ADDR);
    thingsboard_token = decryptString(EEPROM.readString(TB_TOKEN_ADDR));
    config_url = EEPROM.readString(CONFIG_URL_ADDR);
    if (config_url.length() == 0) config_url = CONFIG_FALLBACK_URL;
    config_username = EEPROM.readString(CONFIG_USER_ADDR);
    config_password = decryptString(EEPROM.readString(CONFIG_PASS_ADDR));
    return (wifi_ssid.length() && thingsboard_host.length() && thingsboard_token.length());
}

// ==== CONFIG URL FALLBACK ====
bool fetchConfigFromURL() {
    Serial.println("\n========== Fetching Config from URL ==========");
    Serial.printf("Config URL: %s\n", config_url.c_str());
    
    HTTPClient http;
    http.begin(config_url);
    http.setTimeout(10000); // 10 second timeout to prevent watchdog issues
    
    // Add basic authentication if credentials are provided
    if (config_username.length() > 0 && config_password.length() > 0) {
        http.setAuthorization(config_username.c_str(), config_password.c_str());
        Serial.println("Using authentication for config URL");
    }
    
    // Add device identification
    http.addHeader("X-Device-MAC", WiFi.macAddress());
    http.addHeader("X-Device-Type", "BLE-Gateway");
    http.addHeader("X-Firmware-Version", FIRMWARE_VERSION);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Config received:");
        Serial.println(payload);
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            // Update MQTT configuration if provided
            if (doc.containsKey("mqtt_host")) {
                thingsboard_host = doc["mqtt_host"].as<String>();
                Serial.printf("Updated MQTT host: %s\n", thingsboard_host.c_str());
            }
            if (doc.containsKey("mqtt_token")) {
                thingsboard_token = doc["mqtt_token"].as<String>();
                Serial.println("Updated MQTT token");
            }
            if (doc.containsKey("device_token")) {
                thingsboard_token = doc["device_token"].as<String>();
                Serial.println("Updated device token");
            }
            
            // Save updated config
            saveConfig();
            
            http.end();
            Serial.println("Config updated successfully from URL");
            return true;
        } else {
            Serial.printf("JSON parse error: %s\n", error.c_str());
        }
    } else {
        Serial.printf("HTTP error: %d\n", httpCode);
    }
    
    http.end();
    Serial.println("==========================================\n");
    return false;
}

void initConfigManager() {
    EEPROM.begin(EEPROM_SIZE);
}

#endif // CONFIG_MANAGER_H
