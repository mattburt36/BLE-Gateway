/**
 * BLE Gateway for XIAO ESP32-S3
 * 
 * Multi-threaded BLE to MQTT gateway with smart device tracking
 * - Web-based WiFi/MQTT configuration
 * - Remote configuration via gwconfig.hoptech.co.nz
 * - NTP time synchronization
 * - 12-hour change detection and reporting
 * - Multi-threaded FreeRTOS architecture
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <time.h>
#include <map>

// Firmware version
#define FIRMWARE_VERSION "2.0.0"
#define FIRMWARE_TITLE "BLE-Gateway-XIAO"

// Forward declarations
void startTasks();
void stopTasks();

// Include modular components
#include "config_manager.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "mqtt_handler.h"
#include "device_tracker.h"
#include "ble_scanner.h"

// Global configuration
String wifi_ssid = "";
String wifi_password = "";
String mqtt_host = "mqtt.hoptech.co.nz";
String mqtt_user = "";
String mqtt_password = "";
String device_id = "";
bool wifi_connected = false;
bool mqtt_connected = false;
bool config_mode = false;
bool time_synced = false;
unsigned long current_timestamp = 0;

// Configuration from remote server
String company = "";
String development = "";
String firmware_url = "";

// Task handles
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t trackerTaskHandle = NULL;

// Mutexes for thread safety
SemaphoreHandle_t deviceMapMutex = NULL;
SemaphoreHandle_t mqttMutex = NULL;

// WiFi clients
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WebServer webServer(80);
DNSServer dnsServer;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n========================================");
    Serial.printf("%s v%s\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
    Serial.println("XIAO ESP32-S3 BLE Gateway");
    Serial.println("========================================\n");
    
    // Generate device ID from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    device_id = String(mac[0], HEX) + String(mac[1], HEX) + 
                String(mac[2], HEX) + String(mac[3], HEX) + 
                String(mac[4], HEX) + String(mac[5], HEX);
    device_id.toUpperCase();
    
    Serial.printf("Device ID: %s\n\n", device_id.c_str());
    
    // Initialize configuration manager
    initConfigManager();
    
    // Check if we have stored configuration
    bool hasConfig = loadConfig();
    
    if (!hasConfig) {
        Serial.println("No configuration found in flash.");
        Serial.println("Starting WiFi Access Point for configuration...\n");
        startConfigPortal();
        config_mode = true;
    } else {
        Serial.println("Configuration loaded from flash.");
        Serial.printf("WiFi SSID: %s\n", wifi_ssid.c_str());
        Serial.printf("MQTT Host: %s\n\n", mqtt_host.c_str());
        
        // Try to connect to WiFi
        if (connectWiFi()) {
            wifi_connected = true;
            
            // Sync time with NTP
            if (syncTimeNTP()) {
                time_synced = true;
                Serial.println("Time synchronized via NTP");
            } else {
                Serial.println("NTP sync failed, will try config server for time");
            }
            
            // Fetch remote configuration
            if (fetchRemoteConfig()) {
                Serial.println("Remote configuration retrieved successfully");
            }
            
            // Connect to MQTT
            if (connectMQTT()) {
                mqtt_connected = true;
                Serial.println("MQTT connected successfully\n");
                
                // Start multi-threaded operation
                startTasks();
            } else {
                Serial.println("MQTT connection failed, entering config mode");
                startConfigPortal();
                config_mode = true;
            }
        } else {
            Serial.println("WiFi connection failed, entering config mode");
            startConfigPortal();
            config_mode = true;
        }
    }
    
    Serial.println("Setup complete.\n");
}

void loop() {
    if (config_mode) {
        // Handle web server and DNS for config portal
        webServer.handleClient();
        dnsServer.processNextRequest();
        delay(10);
    } else {
        // Main loop does minimal work - tasks handle everything
        delay(1000);
        
        // Check for critical failures
        if (!wifi_connected || WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost!");
            stopTasks();
            startConfigPortal();
            config_mode = true;
        }
    }
}

void startTasks() {
    Serial.println("Creating FreeRTOS tasks...");
    
    // Create mutexes
    deviceMapMutex = xSemaphoreCreateMutex();
    mqttMutex = xSemaphoreCreateMutex();
    
    if (deviceMapMutex == NULL || mqttMutex == NULL) {
        Serial.println("ERROR: Failed to create mutexes!");
        return;
    }
    
    // Task 1: BLE Scanning (Core 1, Priority 1)
    xTaskCreatePinnedToCore(
        bleScanTask,
        "BLE_Task",
        8192,
        NULL,
        1,
        &bleTaskHandle,
        1
    );
    
    // Task 2: MQTT Maintenance (Core 0, Priority 2)
    xTaskCreatePinnedToCore(
        mqttMaintenanceTask,
        "MQTT_Task",
        8192,
        NULL,
        2,
        &mqttTaskHandle,
        0
    );
    
    // Task 3: WiFi Monitoring (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        wifiMonitorTask,
        "WiFi_Task",
        4096,
        NULL,
        1,
        &wifiTaskHandle,
        0
    );
    
    // Task 4: Device Tracker (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        deviceTrackerTask,
        "Tracker_Task",
        8192,
        NULL,
        1,
        &trackerTaskHandle,
        0
    );
    
    Serial.println("All tasks created successfully!\n");
}

void stopTasks() {
    Serial.println("Stopping all tasks...");
    
    if (bleTaskHandle != NULL) {
        vTaskDelete(bleTaskHandle);
        bleTaskHandle = NULL;
    }
    if (mqttTaskHandle != NULL) {
        vTaskDelete(mqttTaskHandle);
        mqttTaskHandle = NULL;
    }
    if (wifiTaskHandle != NULL) {
        vTaskDelete(wifiTaskHandle);
        wifiTaskHandle = NULL;
    }
    if (trackerTaskHandle != NULL) {
        vTaskDelete(trackerTaskHandle);
        trackerTaskHandle = NULL;
    }
    
    Serial.println("All tasks stopped.");
}
