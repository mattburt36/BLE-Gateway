/**
 * BLE Gateway for SenseCAP Indicator
 * 
 * BLE temperature monitor with display
 * - Touch screen WiFi configuration
 * - NTP time synchronization
 * - Real-time temperature display
 * - Multi-threaded FreeRTOS architecture
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>
#include <map>
#include <lvgl.h>

// Firmware version
#define FIRMWARE_VERSION "3.0.0"
#define FIRMWARE_TITLE "BLE-Gateway-SenseCAP"

// Forward declarations
void startTasks();
void stopTasks();

// Include modular components
#include "config_manager.h"
#include "device_tracker.h"
#include "ble_scanner.h"
#include "display_manager.h"
#include "wifi_manager.h"

// Global configuration
String wifi_ssid = "";
String wifi_password = "";
String device_id = "";
bool wifi_connected = false;
bool time_synced = false;
unsigned long current_timestamp = 0;

// Task handles
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// Mutexes for thread safety
SemaphoreHandle_t deviceMapMutex = NULL;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("========================================");
    Serial.printf("%s v%s\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
    Serial.println("SenseCAP Indicator BLE Gateway");
    Serial.println("========================================");
    
    // Generate device ID from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    device_id = String(mac[0], HEX) + String(mac[1], HEX) + 
                String(mac[2], HEX) + String(mac[3], HEX) + 
                String(mac[4], HEX) + String(mac[5], HEX);
    device_id.toUpperCase();
    
    Serial.printf("Device ID: %s\n\n", device_id.c_str());
    
    // Initialize display first
    Serial.println("Initializing display system...");
    if (!init_display()) {
        Serial.println("Display initialization failed!");
        while(1) delay(1000);
    }
    
    // Show main screen
    create_main_screen();
    show_main_screen();
    
    // Initialize configuration manager
    initConfigManager();
    
    // Check if we have stored configuration
    bool hasConfig = loadConfig();
    
    if (!hasConfig) {
        Serial.println("No configuration found in flash.");
        Serial.println("Showing WiFi configuration screen...\n");
        show_wifi_config();
    } else {
        Serial.println("Configuration loaded from flash.");
        Serial.printf("WiFi SSID: %s\n\n", wifi_ssid.c_str());
        
        // Try to connect to WiFi
        if (connectWiFi()) {
            wifi_connected = true;
            Serial.println("WiFi connected!");
            
            // Sync time with NTP
            if (syncTimeNTP()) {
                time_synced = true;
                Serial.println("Time synchronized via NTP");
            } else {
                Serial.println("NTP sync failed, continuing without time sync");
            }
            
            // Start BLE scanning
            startTasks();
        } else {
            Serial.println("WiFi connection failed!");
            Serial.println("Showing configuration screen\n");
            show_wifi_config();
        }
    }
    
    Serial.println("Setup complete.\n");
}

void loop() {
    // Update display UI
    update_display();
    
    // Main loop - retry connections if needed
    static unsigned long last_retry = 0;
    unsigned long now = millis();
    
    // Check WiFi connection
    if (!wifi_connected || WiFi.status() != WL_CONNECTED) {
        if (now - last_retry > 30000) {  // Retry every 30 seconds
            Serial.println("WiFi disconnected, attempting reconnect...");
            if (connectWiFi()) {
                wifi_connected = true;
                Serial.println("WiFi reconnected!");
                
                // Sync time if not already synced
                if (!time_synced && syncTimeNTP()) {
                    time_synced = true;
                    Serial.println("Time synchronized via NTP");
                }
                
                last_retry = now;
            } else {
                Serial.println("WiFi reconnect failed, will retry");
                last_retry = now;
            }
        }
    }
    
    delay(5); // Small delay for LVGL
}

void startTasks() {
    Serial.println("Creating FreeRTOS tasks...");
    
    // Initialize BLE scanner
    initBLEScanner();
    
    // Create mutex
    deviceMapMutex = xSemaphoreCreateMutex();
    
    if (deviceMapMutex == NULL) {
        Serial.println("ERROR: Failed to create mutex!");
        return;
    }
    
    // Task 1: BLE Scanning (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        bleScanTask,
        "BLE_Task",
        8192,
        NULL,
        1,
        &bleTaskHandle,
        0
    );
    
    // Task 4: Device Tracker (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        deviceTrackerTask,
        "Tracker_Task",
        8192,
        NULL,
        1,
        &bleTaskHandle,
        0
    );
    
    Serial.println("BLE task created successfully!\n");
}

void stopTasks() {
    Serial.println("Stopping all tasks...");
    
    if (bleTaskHandle != NULL) {
        vTaskDelete(bleTaskHandle);
        bleTaskHandle = NULL;
    }
    
    Serial.println("All tasks stopped.");
}
