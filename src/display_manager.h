/**
 * Display Manager for SenseCAP Indicator
 * 
 * Handles:
 * - LVGL initialization and display driver
 * - Touch input
 * - WiFi configuration UI
 * - Temperature sensor display
 * - Status information
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <lvgl.h>
#include <Wire.h>

// External declarations - these are defined in main.cpp and other headers
extern String wifi_ssid;
extern String wifi_password;
extern bool wifi_connected;
extern bool time_synced;
extern TaskHandle_t bleTaskHandle;
extern void saveConfig();
extern bool connectWiFi();
extern bool syncTimeNTP();
extern void startTasks();

// Display configuration
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 480;

// LVGL display buffer (10 lines worth of pixels)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

// UI Objects
static lv_obj_t *main_screen = NULL;
static lv_obj_t *wifi_config_screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *wifi_status_label = NULL;
static lv_obj_t *temp_container = NULL;
static lv_obj_t *ssid_textarea = NULL;
static lv_obj_t *pass_textarea = NULL;
static lv_obj_t *keyboard = NULL;

// Display state
static bool display_initialized = false;
static unsigned long last_ui_update = 0;
const unsigned long UI_UPDATE_INTERVAL = 1000; // Update UI every second

// RGB LCD configuration for ESP32-S3
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

esp_lcd_panel_handle_t panel_handle = NULL;

// RGB interface pins
#define LCD_PIXEL_CLOCK_HZ (16 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_HSYNC          16
#define PIN_NUM_VSYNC          17
#define PIN_NUM_DE             18
#define PIN_NUM_PCLK           21
#define PIN_NUM_DATA0          0  // R0
#define PIN_NUM_DATA1          1  // R1
#define PIN_NUM_DATA2          2  // R2
#define PIN_NUM_DATA3          3  // R3
#define PIN_NUM_DATA4          4  // R4
#define PIN_NUM_DATA5          5  // G0
#define PIN_NUM_DATA6          6  // G1
#define PIN_NUM_DATA7          7  // G2
#define PIN_NUM_DATA8          8  // G3
#define PIN_NUM_DATA9          9  // G4
#define PIN_NUM_DATA10         10 // G5
#define PIN_NUM_DATA11         11 // B0
#define PIN_NUM_DATA12         12 // B1
#define PIN_NUM_DATA13         13 // B2
#define PIN_NUM_DATA14         14 // B3
#define PIN_NUM_DATA15         15 // B4

// I2C for touch controller
#define I2C_SDA 39
#define I2C_SCL 40
#define TOUCH_INT 42

// Touch controller (FT5x06)
#define FT5x06_ADDR 0x38

// PCA9535 I/O Expander for display control
#define PCA9535_ADDR 0x20
#define LCD_RST_BIT 4
#define DISP_EN_BIT 5
#define TP_RST_BIT 6

// Touch data
struct TouchPoint {
    int16_t x;
    int16_t y;
    bool touched;
};

static TouchPoint touch_point = {0, 0, false};

// PCA9535 I/O Expander functions
void pca9535_write_config(uint8_t port, uint8_t value) {
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(0x06 + port); // Config register
    Wire.write(value);
    Wire.endTransmission();
}

void pca9535_write_output(uint8_t port, uint8_t value) {
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(0x02 + port); // Output register
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t pca9535_read_output(uint8_t port) {
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(0x02 + port);
    Wire.endTransmission(false);
    Wire.requestFrom(PCA9535_ADDR, 1);
    return Wire.read();
}

// Initialize I/O Expander and enable display
void init_pca9535() {
    // Configure Port 0 as outputs (bits 0-7)
    pca9535_write_config(0, 0x00);
    
    // Read current output state
    uint8_t output = pca9535_read_output(0);
    
    // Enable display and release resets
    output |= (1 << DISP_EN_BIT);   // Display enable
    output |= (1 << LCD_RST_BIT);   // LCD reset (active low, so high = normal)
    output |= (1 << TP_RST_BIT);    // Touch reset (active low, so high = normal)
    
    pca9535_write_output(0, output);
    delay(120); // Wait for display to initialize
}

// Touch read function for FT5x06
bool read_touch(int16_t &x, int16_t &y) {
    Wire.beginTransmission(FT5x06_ADDR);
    Wire.write(0x02); // Touch data register
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    Wire.requestFrom(FT5x06_ADDR, 4);
    if (Wire.available() < 4) {
        return false;
    }
    
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    
    // Check if touch is valid (event flag in upper 2 bits of xh)
    uint8_t event = (xh >> 6) & 0x03;
    if (event == 0x01 || event == 0x02) { // Touch down or contact
        x = ((xh & 0x0F) << 8) | xl;
        y = ((yh & 0x0F) << 8) | yl;
        return true;
    }
    
    return false;
}

// LVGL display flush callback
static bool display_flush_ready(esp_lcd_panel_handle_t panel, void *user_data) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_data;
    lv_disp_flush_ready(disp_driver);
    return false;
}

void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
}

// LVGL touch input callback
void lvgl_touch_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    int16_t x, y;
    
    if (read_touch(x, y)) {
        touch_point.x = x;
        touch_point.y = y;
        touch_point.touched = true;
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        touch_point.touched = false;
        data->state = LV_INDEV_STATE_REL;
    }
}

// Initialize RGB LCD panel
bool init_rgb_panel() {
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = screenWidth,
            .v_res = screenHeight,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 10,
            .hsync_front_porch = 20,
            .vsync_pulse_width = 10,
            .vsync_back_porch = 10,
            .vsync_front_porch = 10,
            .flags = {
                .pclk_active_neg = false,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = 0,
        .sram_trans_align = 0,
        .psram_trans_align = 64,
        .hsync_gpio_num = PIN_NUM_HSYNC,
        .vsync_gpio_num = PIN_NUM_VSYNC,
        .de_gpio_num = PIN_NUM_DE,
        .pclk_gpio_num = PIN_NUM_PCLK,
        .data_gpio_nums = {
            PIN_NUM_DATA0, PIN_NUM_DATA1, PIN_NUM_DATA2, PIN_NUM_DATA3,
            PIN_NUM_DATA4, PIN_NUM_DATA5, PIN_NUM_DATA6, PIN_NUM_DATA7,
            PIN_NUM_DATA8, PIN_NUM_DATA9, PIN_NUM_DATA10, PIN_NUM_DATA11,
            PIN_NUM_DATA12, PIN_NUM_DATA13, PIN_NUM_DATA14, PIN_NUM_DATA15,
        },
        .disp_gpio_num = -1,
        .flags = {
            .fb_in_psram = 1,
        },
    };
    
    return esp_lcd_new_rgb_panel(&panel_config, &panel_handle) == ESP_OK;
}

// Initialize display system
bool init_display() {
    Serial.println("Initializing display...");
    
    // Initialize I2C for touch and I/O expander
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    
    // Initialize I/O expander and enable display
    init_pca9535();
    
    // Initialize RGB LCD panel
    if (!init_rgb_panel()) {
        Serial.println("Failed to initialize RGB panel");
        return false;
    }
    
    // Initialize LVGL
    lv_init();
    
    // Allocate display buffers in PSRAM
    buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(screenWidth * 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    if (buf1 == NULL || buf2 == NULL) {
        Serial.println("Failed to allocate display buffers");
        return false;
    }
    
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * 10);
    
    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Register touch input device
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);
    
    // Reset panel
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    
    Serial.println("Display initialized successfully");
    display_initialized = true;
    return true;
}

// Create main temperature display screen
void create_main_screen() {
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(main_screen);
    lv_label_set_text(title, "BLE Temperature Monitor");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Time display
    time_label = lv_label_create(main_screen);
    lv_label_set_text(time_label, "Time: --:--:--");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 40);
    
    // WiFi status
    wifi_status_label = lv_label_create(main_screen);
    lv_label_set_text(wifi_status_label, "WiFi: Disconnected");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF0000), 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 65);
    
    // Temperature container (scrollable list)
    temp_container = lv_obj_create(main_screen);
    lv_obj_set_size(temp_container, 440, 340);
    lv_obj_align(temp_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(temp_container, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_color(temp_container, lv_color_hex(0x444444), 0);
    lv_obj_set_flex_flow(temp_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(temp_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(temp_container, 10, 0);
    lv_obj_set_style_pad_row(temp_container, 5, 0);
    
    // Placeholder text
    lv_obj_t *placeholder = lv_label_create(temp_container);
    lv_label_set_text(placeholder, "No sensors detected yet...\nScanning for BLE devices...");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
}

// Forward declarations for event handlers
static void textarea_event_handler(lv_event_t *e);
static void connect_btn_event_handler(lv_event_t *e);
static void keyboard_event_handler(lv_event_t *e);

// Event handler for text area focus
static void textarea_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        // Show keyboard and link it to this text area
        if (keyboard != NULL) {
            lv_keyboard_set_textarea(keyboard, ta);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Event handler for keyboard
static void keyboard_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // Hide keyboard when user presses OK or Cancel
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

// Event handler for Connect button
static void connect_btn_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        Serial.println("Connect button clicked!");
        
        // Get text from text areas
        const char *ssid = lv_textarea_get_text(ssid_textarea);
        const char *pass = lv_textarea_get_text(pass_textarea);
        
        if (strlen(ssid) == 0) {
            Serial.println("Error: SSID is empty");
            // TODO: Show error message on screen
            return;
        }
        
        Serial.printf("Attempting to connect to: %s\n", ssid);
        
        // Save credentials to global variables
        wifi_ssid = String(ssid);
        wifi_password = String(pass);
        
        // Save to flash
        saveConfig();
        Serial.println("WiFi credentials saved to flash");
        
        // Attempt WiFi connection
        if (connectWiFi()) {
            wifi_connected = true;
            Serial.println("WiFi connected successfully!");
            
            // Sync time
            if (syncTimeNTP()) {
                time_synced = true;
                Serial.println("Time synchronized via NTP");
            }
            
            // Show main screen
            show_main_screen();
            
            // Start BLE scanning if not already started
            extern TaskHandle_t bleTaskHandle;
            if (bleTaskHandle == NULL) {
                extern void startTasks();
                startTasks();
            }
        } else {
            Serial.println("WiFi connection failed!");
            // TODO: Show error message on screen
            // For now, stay on config screen so user can try again
        }
    }
}

// Create WiFi configuration screen
void create_wifi_config_screen() {
    wifi_config_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_config_screen, lv_color_hex(0x1A1A1A), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(wifi_config_screen);
    lv_label_set_text(title, "WiFi Configuration");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // SSID Label
    lv_obj_t *ssid_label = lv_label_create(wifi_config_screen);
    lv_label_set_text(ssid_label, "Network SSID:");
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 20, 70);
    
    // SSID Text Area
    ssid_textarea = lv_textarea_create(wifi_config_screen);
    lv_obj_set_size(ssid_textarea, 440, 50);
    lv_obj_align(ssid_textarea, LV_ALIGN_TOP_MID, 0, 95);
    lv_textarea_set_placeholder_text(ssid_textarea, "Enter WiFi SSID");
    lv_textarea_set_one_line(ssid_textarea, true);
    lv_obj_add_event_cb(ssid_textarea, textarea_event_handler, LV_EVENT_ALL, NULL);
    
    // Password Label
    lv_obj_t *pass_label = lv_label_create(wifi_config_screen);
    lv_label_set_text(pass_label, "Password:");
    lv_obj_set_style_text_color(pass_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(pass_label, LV_ALIGN_TOP_LEFT, 20, 160);
    
    // Password Text Area
    pass_textarea = lv_textarea_create(wifi_config_screen);
    lv_obj_set_size(pass_textarea, 440, 50);
    lv_obj_align(pass_textarea, LV_ALIGN_TOP_MID, 0, 185);
    lv_textarea_set_placeholder_text(pass_textarea, "Enter WiFi password");
    lv_textarea_set_password_mode(pass_textarea, true);
    lv_textarea_set_one_line(pass_textarea, true);
    lv_obj_add_event_cb(pass_textarea, textarea_event_handler, LV_EVENT_ALL, NULL);
    
    // Connect Button
    lv_obj_t *connect_btn = lv_btn_create(wifi_config_screen);
    lv_obj_set_size(connect_btn, 200, 50);
    lv_obj_align(connect_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(connect_btn, connect_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_t *btn_label = lv_label_create(connect_btn);
    lv_label_set_text(btn_label, "Connect");
    lv_obj_center(btn_label);
    
    // Keyboard (hidden initially)
    keyboard = lv_keyboard_create(wifi_config_screen);
    lv_obj_set_size(keyboard, LV_HOR_RES, LV_VER_RES / 2);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keyboard, keyboard_event_handler, LV_EVENT_ALL, NULL);
}

// Update temperature display with device data
void update_temperature_display() {
    if (!display_initialized || temp_container == NULL) return;
    
    // Clear existing items
    lv_obj_clean(temp_container);
    
    // Access device map with mutex
    if (xSemaphoreTake(deviceMapMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (deviceMap.empty()) {
            lv_obj_t *placeholder = lv_label_create(temp_container);
            lv_label_set_text(placeholder, "No sensors detected...");
            lv_obj_set_style_text_color(placeholder, lv_color_hex(0x888888), 0);
        } else {
            for (auto& pair : deviceMap) {
                DeviceData& device = pair.second;
                
                // Create card for each device
                lv_obj_t *card = lv_obj_create(temp_container);
                lv_obj_set_size(card, 420, 80);
                lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
                lv_obj_set_style_border_color(card, lv_color_hex(0x555555), 0);
                lv_obj_set_style_radius(card, 8, 0);
                
                // Device name/MAC
                lv_obj_t *name_label = lv_label_create(card);
                String label_text = device.name.isEmpty() ? device.mac : device.name;
                lv_label_set_text(name_label, label_text.c_str());
                lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_text_font(name_label, &lv_font_montserrat_16, 0);
                lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 10, 5);
                
                // Temperature and Humidity
                if (device.temperature != 0.0 || device.humidity != 0.0) {
                    char temp_str[64];
                    snprintf(temp_str, sizeof(temp_str), "%.1f°C  %.1f%%RH", 
                             device.temperature, device.humidity);
                    lv_obj_t *temp_label = lv_label_create(card);
                    lv_label_set_text(temp_label, temp_str);
                    lv_obj_set_style_text_color(temp_label, lv_color_hex(0x00FF00), 0);
                    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_18, 0);
                    lv_obj_align(temp_label, LV_ALIGN_BOTTOM_LEFT, 10, -5);
                }
                
                // RSSI
                char rssi_str[16];
                snprintf(rssi_str, sizeof(rssi_str), "%d dBm", device.rssi);
                lv_obj_t *rssi_label = lv_label_create(card);
                lv_label_set_text(rssi_label, rssi_str);
                lv_obj_set_style_text_color(rssi_label, lv_color_hex(0xAAAAAA), 0);
                lv_obj_align(rssi_label, LV_ALIGN_TOP_RIGHT, -10, 5);
            }
        }
        xSemaphoreGive(deviceMapMutex);
    }
}

// Update time display
void update_time_display() {
    if (!display_initialized || !time_synced || time_label == NULL) return;
    
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "Time: %I:%M:%S %p", &timeinfo);
    lv_label_set_text(time_label, time_str);
}

// Update WiFi status display
void update_wifi_status() {
    if (!display_initialized || wifi_status_label == NULL) return;
    
    if (wifi_connected) {
        lv_label_set_text(wifi_status_label, ("WiFi: " + wifi_ssid).c_str());
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(wifi_status_label, "WiFi: Disconnected");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF0000), 0);
    }
}

// Show WiFi configuration screen
void show_wifi_config() {
    if (!display_initialized) return;
    
    if (wifi_config_screen == NULL) {
        create_wifi_config_screen();
    }
    lv_scr_load(wifi_config_screen);
}

// Show main screen
void show_main_screen() {
    if (!display_initialized) return;
    
    if (main_screen == NULL) {
        create_main_screen();
    }
    lv_scr_load(main_screen);
}

// Main display update function (call from loop)
void update_display() {
    if (!display_initialized) return;
    
    unsigned long now = millis();
    if (now - last_ui_update < UI_UPDATE_INTERVAL) {
        lv_timer_handler();
        return;
    }
    
    last_ui_update = now;
    
    update_time_display();
    update_wifi_status();
    update_temperature_display();
    
    lv_timer_handler();
}

#endif // DISPLAY_MANAGER_H
