# Arduino Setup Guide for SenseCAP Indicator D1

## Prerequisites

### Arduino IDE Setup
1. Install **Arduino IDE** (v2.0+ recommended)
2. Add ESP32 board support:
   - Go to **File > Preferences**
   - Add to "Additional Boards Manager URLs":
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Go to **Tools > Board > Boards Manager**
   - Search for "esp32" and install latest version

### Board Selection
- **Board**: ESP32S3 Dev Module
- **PSRAM**: OPI PSRAM (enabled)
- **Flash Size**: 8MB
- **Partition Scheme**: Huge APP (3MB No OTA)
- **Upload Speed**: 921600

## Required Libraries

### Install via Library Manager
1. **LVGL** - UI/Graphics library (v8.3.0+)
   - Search: "lvgl" by LVGL
2. **TFT_eSPI** - Display driver (v2.5.0+)
   - Search: "TFT_eSPI" by Bodmer
3. **ArduinoJson** - JSON parsing (v7.0.0+)
   - Search: "ArduinoJson" by Benoit Blanchon
4. **PubSubClient** - MQTT client (v2.8+)
   - Search: "PubSubClient" by Nick O'Leary

### Manual Installation (GitHub)
1. **TouchLib** - FT5x06 touch controller
   - Clone/download: [TouchLib Repository]
   - Place in Arduino/libraries/

2. **PCA9535** - I/O Expander library
   - Clone/download: [PCA9535 Library]
   - Place in Arduino/libraries/

## TFT_eSPI Configuration

### Method 1: User_Setup.h
Navigate to: `Arduino/libraries/TFT_eSPI/User_Setup.h`

Add this configuration:

```cpp
// Display driver
#define ST7701_DRIVER

// Display resolution
#define TFT_WIDTH  480
#define TFT_HEIGHT 480

// SPI pins (for initialization only - display uses RGB parallel)
#define TFT_MISO  -1      // Not used for RGB interface
#define TFT_MOSI  48      
#define TFT_SCLK  41
#define TFT_CS    45      // Via PCA9535
#define TFT_DC    21      
#define TFT_RST   15      // Via PCA9535

// RGB interface pins are handled by ESP32-S3 LCD peripheral
// See display_config.h for full RGB pin mapping

// Touch is handled via I2C (FT5x06)
#define TOUCH_CS  -1      // I2C interface
```

### Method 2: Custom Setup File
Create `User_Setup_SenseCAP.h` with above config, then in `User_Setup_Select.h`:

```cpp
#include <User_Setup_SenseCAP.h>
```

## LVGL Configuration

Create `lv_conf.h` in your sketch folder or Arduino/libraries/:

```cpp
#define LV_CONF_INCLUDE_SIMPLE
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)    // 48KB
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 1
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
```

## PlatformIO Configuration

If using PlatformIO instead of Arduino IDE:

```ini
[env:sensecap_indicator]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_build.partitions = huge_app.csv
board_build.flash_mode = dio
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_size = 8MB
upload_speed = 921600

lib_deps = 
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^7.0.0
    lvgl/lvgl@^8.3.0
    bodmer/TFT_eSPI@^2.5.0

build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DSENSECAP_INDICATOR
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_LVGL_H_INCLUDE_SIMPLE
```

## Basic Example Sketch

### Minimal Display Test

```cpp
#include <lvgl.h>
#include <TFT_eSPI.h>

static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 480;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

// LVGL Display flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    
    lv_disp_flush_ready(disp);
}

// LVGL Touch input callback
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY, 600);
    
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize LVGL
    lv_init();
    
    // Initialize TFT
    tft.begin();
    tft.setRotation(0); // Portrait
    
    // Touch calibration data (adjust for your unit)
    uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
    tft.setTouch(calData);
    
    // Setup LVGL display buffer
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);
    
    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Register touch input device
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Create simple UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello SenseCAP!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    Serial.println("Display initialized!");
}

void loop() {
    lv_timer_handler(); // Handle LVGL tasks
    delay(5);
}
```

## Troubleshooting

### Display Issues
- **Blank screen**: Check power supply (5V/1A minimum)
- **Wrong colors**: Verify `LV_COLOR_16_SWAP` setting
- **Flickering**: Increase buffer size in `lv_disp_draw_buf_init()`

### Touch Issues
- **No response**: Run touch calibration sketch
- **Inaccurate**: Adjust calibration data array
- **Erratic**: Check I2C connections and grounding

### Memory Issues
- **Compile errors**: Enable PSRAM in board settings
- **Crashes**: Reduce `LV_MEM_SIZE` or display buffer
- **Slow UI**: Increase CPU frequency to 240MHz

## UI Design Tools

### SquareLine Studio
- Visual LVGL UI designer
- Export directly to Arduino code
- Download: [SquareLine Studio](https://squareline.io/)
- Tutorial: [Seeed Wiki - Create Your Own UI](https://wiki.seeedstudio.com/SenseCAP_Indicator_How_to_Create_your_own_UI/)

## Additional Resources

- [LVGL Documentation](https://docs.lvgl.io/)
- [TFT_eSPI Setup Guide](https://github.com/Bodmer/TFT_eSPI)
- [SenseCAP GitHub Examples](https://github.com/Seeed-Solution/sensecap_indicator_esp32)
- [ESP32-S3 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/)
