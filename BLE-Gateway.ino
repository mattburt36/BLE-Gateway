#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <map>
#include <time.h>
#include "mbedtls/aes.h"

// Firmware version - update this with each release
#define FIRMWARE_VERSION "1.1.0"
#define FIRMWARE_TITLE "BLE-Gateway"

// Include modular components
#include "config_manager.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "ble_scanner.h"
#include "mqtt_handler.h"

// Global configuration variables (defined here, declared as extern in modules)
String wifi_ssid = "";
String wifi_password = "";
String thingsboard_host = "";
String thingsboard_token = "";
String config_url = CONFIG_FALLBACK_URL;
String config_username = "";
String config_password = "";
bool wifi_connected = false;
bool mqtt_connected = false;
bool config_mode = false;
bool time_synced = false;

// Task handles for FreeRTOS
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t processingTaskHandle = NULL;

// Mutex for thread-safe access to shared resources
SemaphoreHandle_t detectionBufferMutex = NULL;
SemaphoreHandle_t mqttMutex = NULL;

// BLE
BLEScan* pBLEScan;
unsigned long last_ble_scan = 0;

// OTA State (defined here, declared as extern in modules)
OTAState otaState;

// Detection buffer (defined here, declared as extern in modules)
std::map<String, BLEAdvertEntry> detectionBuffer;
unsigned long lastBatchSend = 0;
const unsigned long BATCH_INTERVAL = 60000;

unsigned long mqttFailStart = 0;
bool apModeOffered = false;

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
DNSServer dnsServer;


// ==== SETUP & LOOP ====
void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n========================================");
    Serial.printf("BLE Gateway Starting...\n");
    Serial.printf("Firmware: %s v%s\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
    Serial.println("========================================\n");
    
    initConfigManager();

    // Initialize OTA state
    otaState.updateAvailable = false;
    otaState.updateInProgress = false;
    otaState.progressPercent = 0;
    otaState.status = "IDLE";

    bool configOk = loadConfig();
    if (configOk) {
        Serial.println("Config loaded, trying WiFi and MQTT...");
        if (tryWiFi() && tryMQTT()) {
            config_mode = false;
            Serial.println("WiFi & MQTT OK, starting BLE scan...");
        } else {
            Serial.println("WiFi or MQTT failed, entering config mode...");
            setupAP();
            setupWebServer();
        }
    } else {
        Serial.println("No valid config, entering config mode...");
        setupAP();
        setupWebServer();
    }

    client.setBufferSize(16384);

    initBLEScanner();
    
    // Create mutexes for thread-safe access
    detectionBufferMutex = xSemaphoreCreateMutex();
    mqttMutex = xSemaphoreCreateMutex();
    
    if (detectionBufferMutex == NULL || mqttMutex == NULL) {
        Serial.println("ERROR: Failed to create mutexes!");
    }
    
    // Create FreeRTOS tasks if not in config mode
    if (!config_mode && wifi_connected) {
        Serial.println("\n========== Starting FreeRTOS Tasks ==========");
        
        // Task 1: MQTT Maintenance (Core 0, Priority 2)
        xTaskCreatePinnedToCore(
            mqttMaintenanceTask,   // Task function
            "MQTT_Task",           // Task name
            8192,                  // Stack size
            NULL,                  // Parameters
            2,                     // Priority
            &mqttTaskHandle,       // Task handle
            0                      // Core 0
        );
        
        // Task 2: BLE Scanning (Core 1, Priority 1)
        xTaskCreatePinnedToCore(
            bleScanTask,           // Task function
            "BLE_Task",            // Task name
            8192,                  // Stack size
            NULL,                  // Parameters
            1,                     // Priority
            &bleTaskHandle,        // Task handle
            1                      // Core 1
        );
        
        // Task 3: Message Processing (Core 0, Priority 1)
        xTaskCreatePinnedToCore(
            messageProcessingTask, // Task function
            "Processing_Task",     // Task name
            8192,                  // Stack size
            NULL,                  // Parameters
            1,                     // Priority
            &processingTaskHandle, // Task handle
            0                      // Core 0
        );
        
        Serial.println("All tasks created successfully!");
        Serial.println("==========================================\n");
    }
    
    Serial.println("Setup complete.");
}

void loop() {
    // Handle config mode
    if (config_mode) {
        server.handleClient();
        dnsServer.processNextRequest();
        delay(10);
        return;
    }

    // Check WiFi connection
    if (!wifi_connected || WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost in main loop, entering config mode...");
        
        // Delete tasks if they exist
        if (mqttTaskHandle != NULL) {
            vTaskDelete(mqttTaskHandle);
            mqttTaskHandle = NULL;
        }
        if (bleTaskHandle != NULL) {
            vTaskDelete(bleTaskHandle);
            bleTaskHandle = NULL;
        }
        if (processingTaskHandle != NULL) {
            vTaskDelete(processingTaskHandle);
            processingTaskHandle = NULL;
        }
        
        setupAP();
        setupWebServer();
        return;
    }

    // Check if MQTT has been down too long
    if (!mqtt_connected && mqttFailStart > 0 && !apModeOffered) {
        if (millis() - mqttFailStart > MQTT_FAIL_AP_TIMEOUT) {
            Serial.println("MQTT failed for over 5 minutes, offering AP config mode...");
            
            // Delete tasks
            if (mqttTaskHandle != NULL) {
                vTaskDelete(mqttTaskHandle);
                mqttTaskHandle = NULL;
            }
            if (bleTaskHandle != NULL) {
                vTaskDelete(bleTaskHandle);
                bleTaskHandle = NULL;
            }
            if (processingTaskHandle != NULL) {
                vTaskDelete(processingTaskHandle);
                processingTaskHandle = NULL;
            }
            
            setupAP();
            setupWebServer();
            apModeOffered = true;
            return;
        }
    }

    // Main loop just yields to tasks
    delay(100);
}