/**
 * Configuration Manager
 * 
 * Handles:
 * - Flash storage (Preferences API)
 * - Configuration load/save
 * - Encryption (if needed)
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>

extern String wifi_ssid;
extern String wifi_password;
extern String mqtt_host;
extern String mqtt_user;
extern String mqtt_password;
extern String device_id;
extern String company;
extern String development;
extern String firmware_url;
extern bool time_synced;
extern unsigned long current_timestamp;

Preferences preferences;

void initConfigManager() {
    preferences.begin("gateway", false);
    Serial.println("Configuration manager initialized.");
}

bool loadConfig() {
    wifi_ssid = preferences.getString("wifi_ssid", "");
    wifi_password = preferences.getString("wifi_pass", "");
    mqtt_host = preferences.getString("mqtt_host", "mqtt.hoptech.co.nz");
    mqtt_user = preferences.getString("mqtt_user", "");
    mqtt_password = preferences.getString("mqtt_pass", "");
    
    // Configuration is valid if we have WiFi credentials
    bool valid = (wifi_ssid.length() > 0 && wifi_password.length() > 0);
    
    if (valid) {
        Serial.println("✓ Configuration loaded successfully");
    } else {
        Serial.println("✗ No valid configuration found");
    }
    
    return valid;
}

void saveConfig() {
    preferences.putString("wifi_ssid", wifi_ssid);
    preferences.putString("wifi_pass", wifi_password);
    preferences.putString("mqtt_host", mqtt_host);
    preferences.putString("mqtt_user", mqtt_user);
    preferences.putString("mqtt_pass", mqtt_password);
    
    Serial.println("✓ Configuration saved to flash");
}

void clearConfig() {
    preferences.clear();
    Serial.println("✓ Configuration cleared from flash");
}

#endif // CONFIG_MANAGER_H
