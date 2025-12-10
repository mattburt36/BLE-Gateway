/**
 * Serial Provisioning Handler
 * 
 * Handles serial commands for device provisioning:
 * - PROVISION:<username>:<password> - Store MQTT credentials
 * - STATUS - Show current configuration status
 * - CLEAR - Clear stored credentials
 */

#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <Arduino.h>

extern String mqtt_user;
extern String mqtt_password;
extern String device_token;
extern Preferences preferences;

// Forward declaration for OTA function
bool performOTA(const String& firmwareUrl, int expectedSize);

void handleSerialProvisioning() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.length() == 0) {
            return;
        }
        
        Serial.printf("\n[PROVISION] Received command: %s\n", command.c_str());
        
        // PROVISION command format: PROVISION:<username>:<password>:<token>
        if (command.startsWith("PROVISION:")) {
            String params = command.substring(10);  // Remove "PROVISION:"
            
            int firstColon = params.indexOf(':');
            int secondColon = params.indexOf(':', firstColon + 1);
            
            if (firstColon > 0 && secondColon > firstColon) {
                // Extract username, password, and optional token
                String user = params.substring(0, firstColon);
                String pass = params.substring(firstColon + 1, secondColon);
                String token = params.substring(secondColon + 1);
                
                if (user.length() > 0 && pass.length() > 0) {
                    Serial.println("\n[PROVISION] Storing credentials in encrypted flash...");
                    
                    mqtt_user = user;
                    mqtt_password = pass;
                    if (token.length() > 0) {
                        device_token = token;
                    }
                    
                    // Save to encrypted NVS
                    preferences.putString("mqtt_user", mqtt_user);
                    preferences.putString("mqtt_pass", mqtt_password);
                    if (token.length() > 0) {
                        preferences.putString("device_token", device_token);
                    }
                    
                    Serial.println("✓ Credentials stored successfully!");
                    Serial.printf("  Username: %s\n", mqtt_user.c_str());
                    Serial.println("  Password: ***ENCRYPTED***");
                    if (token.length() > 0) {
                        Serial.println("  Token: ***SET***");
                    }
                    Serial.println("\n[PROVISION] Device needs reboot to apply changes");
                    Serial.println("           Send REBOOT command or power cycle the device\n");
                } else {
                    Serial.println("✗ Invalid credentials - username and password required");
                }
            } else {
                Serial.println("✗ Invalid format. Use: PROVISION:<username>:<password>:<token>");
            }
        }
        // STATUS command
        else if (command.equalsIgnoreCase("STATUS")) {
            Serial.println("\n========== DEVICE STATUS ==========");
            Serial.printf("Device ID: %s\n", device_id.c_str());
            Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
            Serial.printf("WiFi SSID: %s\n", wifi_ssid.length() > 0 ? wifi_ssid.c_str() : "(not configured)");
            Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
            Serial.printf("MQTT Broker: %s\n", mqtt_host.c_str());
            Serial.printf("MQTT User: %s\n", mqtt_user.length() > 0 ? mqtt_user.c_str() : "(not provisioned)");
            Serial.printf("MQTT Password: %s\n", mqtt_password.length() > 0 ? "***SET***" : "(not provisioned)");
            Serial.printf("MQTT Status: %s\n", mqtt_connected ? "Connected" : "Disconnected");
            Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
            Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
            Serial.println("===================================\n");
        }
        // CLEAR command
        else if (command.equalsIgnoreCase("CLEAR")) {
            Serial.println("\n[PROVISION] Clearing all stored credentials...");
            preferences.clear();
            Serial.println("✓ All credentials cleared from flash");
            Serial.println("  Device needs reboot to apply changes\n");
        }
        // REBOOT command
        else if (command.equalsIgnoreCase("REBOOT")) {
            Serial.println("\n[PROVISION] Rebooting device in 2 seconds...\n");
            delay(2000);
            ESP.restart();
        }
        // OTA command - format: OTA:<url>
        else if (command.startsWith("OTA:")) {
            String url = command.substring(4);  // Remove "OTA:"
            url.trim();
            
            if (url.length() > 0 && (url.startsWith("http://") || url.startsWith("https://"))) {
                Serial.printf("\n[OTA] Starting OTA update from: %s\n", url.c_str());
                performOTA(url, 0);
            } else {
                Serial.println("✗ Invalid OTA URL. Must start with http:// or https://");
                Serial.println("  Example: OTA:http://192.168.1.100:8080/firmware.bin");
            }
        }
        // HELP command
        else if (command.equalsIgnoreCase("HELP")) {
            Serial.println("\n========== PROVISIONING COMMANDS ==========");
            Serial.println("PROVISION:<user>:<pass>:<token> - Store MQTT credentials");
            Serial.println("  Example: PROVISION:ble-gateway-ABC123:mypassword:token123");
            Serial.println("");
            Serial.println("STATUS - Show device status and configuration");
            Serial.println("CLEAR  - Clear all stored credentials");
            Serial.println("REBOOT - Reboot the device");
            Serial.println("OTA:<url> - Trigger OTA firmware update");
            Serial.println("  Example: OTA:http://192.168.1.100:8080/firmware.bin");
            Serial.println("HELP   - Show this help message");
            Serial.println("===========================================\n");
        }
        else {
            Serial.println("✗ Unknown command. Send HELP for available commands\n");
        }
    }
}

#endif // PROVISIONING_H
