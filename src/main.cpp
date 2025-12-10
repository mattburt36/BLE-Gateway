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

// LED Pin (built-in LED on most ESP32-S3 boards)
#define LED_PIN 21  // Adjust if needed for your board

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
#include "provisioning.h"

// Global configuration
String wifi_ssid = "";
String wifi_password = "";
String mqtt_host = "mqtt.hoptech.co.nz";  // RabbitMQ MQTT broker
String mqtt_user = "";
String mqtt_password = "";
String device_id = "";
String device_token = "";  // Device authentication token for config server
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
WiFiClient mqttPlainClient;  // Use plain client for testing port 1883
PubSubClient mqttClient(mqttPlainClient);
WebServer webServer(80);
DNSServer dnsServer;

void setup() {
    // Setup LED for status indication
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Blink 3 times to show device is starting
    for(int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }
    
    Serial.begin(115200);
    Serial.println("========================================");
    Serial.printf("%s v%s\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
    Serial.println("XIAO ESP32-S3 BLE Gateway");
    Serial.println("========================================");
    
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
        // LED: Slow blink = AP mode
        digitalWrite(LED_PIN, HIGH);
        startConfigPortal();
        config_mode = true;
    } else {
        Serial.println("Configuration loaded from flash.");
        Serial.printf("WiFi SSID: %s\n", wifi_ssid.c_str());
        Serial.printf("MQTT Host: %s\n", mqtt_host.c_str());
        Serial.printf("MQTT User: %s\n\n", mqtt_user.c_str());
        
        // Try to connect to WiFi
        if (connectWiFi()) {
            wifi_connected = true;
            // LED: 2 quick blinks = WiFi connected
            for(int i = 0; i < 2; i++) {
                digitalWrite(LED_PIN, HIGH);
                delay(100);
                digitalWrite(LED_PIN, LOW);
                delay(100);
            }
            
            // Sync time with NTP
            if (syncTimeNTP()) {
                time_synced = true;
                Serial.println("Time synchronized via NTP");
            } else {
                Serial.println("NTP sync failed, continuing without time sync");
            }
            
            // Try to connect to MQTT
            if (connectMQTT()) {
                mqtt_connected = true;
                Serial.println("MQTT connected successfully\n");
                // LED: Solid ON = MQTT connected and operational
                digitalWrite(LED_PIN, HIGH);
                
                // Start multi-threaded operation
                startTasks();
            } else {
                Serial.println("MQTT connection failed, will retry in loop");
                // Don't start config portal - WiFi is working, just keep retrying MQTT
            }
        } else {
            // WiFi connection failed - go back to AP mode to fix credentials
            Serial.println("WiFi connection failed!");
            Serial.println("Starting AP mode - please check credentials\n");
            startConfigPortal();
            config_mode = true;
        }
    }
    
    Serial.println("Setup complete.\n");
}

void loop() {
    // Handle serial provisioning commands
    handleSerialProvisioning();
    
    if (config_mode) {
        // Handle web server and DNS for config portal
        webServer.handleClient();
        dnsServer.processNextRequest();
        delay(10);
    } else {
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
                    last_retry = now;
                } else {
                    Serial.println("WiFi reconnect failed, will retry");
                    last_retry = now;
                }
            }
        }
        
        // Check MQTT connection
        if (wifi_connected && !mqtt_connected) {
            if (now - last_retry > 10000) {  // Retry every 10 seconds
                Serial.println("\n🔄 ========== MQTT RETRY ATTEMPT ==========");
                Serial.printf("⏱  Uptime: %lu seconds\n", millis() / 1000);
                Serial.printf("   MQTT User: %s\n", mqtt_user.c_str());
                Serial.printf("   MQTT Password: %s\n", mqtt_password.length() > 0 ? "***SET***" : "MISSING");
                
                // Try MQTT connection
                if (connectMQTT()) {
                    mqtt_connected = true;
                    Serial.println("✅ MQTT connected - starting tasks...");
                    startTasks();
                } else {
                    Serial.println("❌ MQTT connection failed - will retry in 10 seconds");
                }
                Serial.println("==========================================\n");
                last_retry = now;
            }
        }
        
        delay(1000);
    }
}

void startTasks() {
    Serial.println("Creating FreeRTOS tasks...");
    
    // Initialize BLE scanner
    initBLEScanner();
    
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
