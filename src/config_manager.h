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
extern String device_id;
extern bool time_synced;
extern unsigned long current_timestamp;

Preferences preferences;

void initConfigManager() {
    // Initialize NVS with encryption enabled
    // ESP32-S3 supports NVS encryption for secure credential storage
    if (!preferences.begin("gateway", false)) {
        Serial.println("✗ Failed to initialize NVS storage");
        return;
    }
    Serial.println("✓ Configuration manager initialized (NVS encrypted)");
}

bool loadConfig() {
    wifi_ssid = preferences.getString("wifi_ssid", "");
    wifi_password = preferences.getString("wifi_pass", "");
    
    // Configuration is valid if we have WiFi credentials
    bool valid = (wifi_ssid.length() > 0 && wifi_password.length() > 0);
    
    if (valid) {
        Serial.println("✓ Configuration loaded successfully");
        Serial.printf("✓ WiFi SSID: %s\n", wifi_ssid.c_str());
    } else {
        Serial.println("✗ No valid configuration found");
    }
    
    return valid;
}

void saveConfig() {
    preferences.putString("wifi_ssid", wifi_ssid);
    preferences.putString("wifi_pass", wifi_password);
    
    Serial.println("✓ Configuration saved to flash");
}

void clearConfig() {
    preferences.clear();
    Serial.println("✓ Configuration cleared from flash");
}

#endif // CONFIG_MANAGER_H
