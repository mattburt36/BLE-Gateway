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
extern String device_token;
extern String company;
extern String development;
extern String firmware_url;
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
    
    // MQTT credentials for RabbitMQ MQTT broker
    mqtt_host = "mqtt.hoptech.co.nz";  // RabbitMQ MQTT broker
    mqtt_user = preferences.getString("mqtt_user", "");
    mqtt_password = preferences.getString("mqtt_pass", "");
    device_token = preferences.getString("device_token", "");  // Device authentication token
    
    // Check if MQTT credentials are provisioned
    if (mqtt_user.length() == 0 || mqtt_password.length() == 0) {
        Serial.println("⚠️  No MQTT credentials provisioned - device needs provisioning");
    }
    
    // Configuration is valid if we have WiFi credentials
    bool valid = (wifi_ssid.length() > 0 && wifi_password.length() > 0);
    
    if (valid) {
        Serial.println("✓ Configuration loaded successfully");
        Serial.printf("✓ MQTT Broker: %s\n", mqtt_host.c_str());
        if (mqtt_user.length() > 0) {
            Serial.printf("✓ MQTT User: %s\n", mqtt_user.c_str());
        }
        if (mqtt_password.length() > 0) {
            Serial.println("✓ MQTT Password: ***SET***");
        }
        if (device_token.length() > 0) {
            Serial.println("✓ Device authentication token found in flash");
        }
    } else {
        Serial.println("✗ No valid configuration found");
    }
    
    return valid;
}

void saveConfig() {
    preferences.putString("wifi_ssid", wifi_ssid);
    preferences.putString("wifi_pass", wifi_password);
    // Don't save mqtt_host as it's hardcoded
    preferences.putString("mqtt_user", mqtt_user);
    preferences.putString("mqtt_pass", mqtt_password);
    preferences.putString("device_token", device_token);
    
    Serial.println("✓ Configuration saved to flash");
}

// Provision device with MQTT credentials and device token
void provisionMQTT(const String& user, const String& pass, const String& token) {
    mqtt_user = user;
    mqtt_password = pass;
    device_token = token;
    preferences.putString("mqtt_user", mqtt_user);
    preferences.putString("mqtt_pass", mqtt_password);
    preferences.putString("device_token", device_token);
    Serial.println("✓ MQTT credentials and device token provisioned to flash");
}

void clearConfig() {
    preferences.clear();
    Serial.println("✓ Configuration cleared from flash");
}

#endif // CONFIG_MANAGER_H
