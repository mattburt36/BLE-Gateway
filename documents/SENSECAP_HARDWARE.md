# SenseCAP Indicator D1 Hardware Documentation

## Overview
The Seeed Studio SenseCAP Indicator D1 is an advanced IoT development platform featuring:
- **Display**: 4-inch capacitive touch screen (480x480 RGB)
- **Dual MCUs**: ESP32-S3 + RP2040
- **Connectivity**: Wi-Fi, Bluetooth 5.0 LE, optional LoRa
- **Expansion**: 2x Grove connectors, SD card slot

## Technical Specifications

### Display System
- **Panel**: 4-inch RGB capacitive touch screen
- **Resolution**: 480 x 480 pixels
- **Controller**: ST7701S
- **Touch Controller**: FT5x06
- **Interface**: RGB 8-bit parallel (high-speed refresh)

### ESP32-S3 (Primary MCU)
- **Processor**: Xtensa® dual-core 32-bit LX7
- **Clock Speed**: Up to 240MHz
- **Flash**: 8MB
- **PSRAM**: 8MB OPI PSRAM
- **Wireless**: 
  - Wi-Fi: 802.11b/g/n, 2.4 GHz
  - Bluetooth: 5.0 LE
- **USB**: Type-C (2 ports)

### RP2040 (Secondary MCU)
- **Processor**: Dual ARM Cortex-M0+
- **Clock Speed**: Up to 133MHz
- **Flash**: 2MB
- **Role**: Sensor management, storage, buzzer control
- **Communication**: UART to ESP32-S3 (COBS format)

### Connectivity & Expansion
- **LoRa**: Optional Semtech SX1262 (862-930 MHz)
- **Grove Connectors**: 2x (support I2C, ADC, PWM, GPIO)
- **Compatible**: 400+ Grove sensors
- **Storage**: Micro SD Card (up to 32GB)
- **Buzzer**: MLT-8530, Resonant Frequency: 2700Hz

### I/O Expander
- **Chip**: PCA9535PW
- **I2C Address**: 0x20
- **Pins**: 16-bit GPIO expansion

## ESP32-S3 Pin Mapping

### Display Interface (ST7701S - RGB 8-bit Parallel)

#### RGB Data Lines
| Channel | Pins          |
|---------|---------------|
| Red     | GPIO0-4       |
| Green   | GPIO5-10      |
| Blue    | GPIO11-15     |

#### Display Control Signals
| Signal  | GPIO   | Description              |
|---------|--------|--------------------------|
| H-Sync  | GPIO16 | Horizontal Sync          |
| V-Sync  | GPIO17 | Vertical Sync            |
| DE      | GPIO18 | Data Enable              |
| PCLK    | GPIO21 | Pixel Clock              |
| RESET   | Exp/5  | Display Reset (via PCA9535) |
| CS      | Exp/4  | Chip Select (via PCA9535)   |

### Communication Buses

#### SPI
| Signal | GPIO   | Purpose                    |
|--------|--------|----------------------------|
| CLK    | GPIO41 | SPI Clock                  |
| MOSI   | GPIO48 | Master Out Slave In        |
| MISO   | GPIO47 | Master In Slave Out (LoRa) |

#### I2C
| Signal | GPIO   | Purpose              |
|--------|--------|----------------------|
| SDA    | GPIO39 | I2C Data             |
| SCL    | GPIO40 | I2C Clock            |

### Other Signals
| Signal  | GPIO   | Description          |
|---------|--------|----------------------|
| IO_INT  | GPIO42 | I/O Expander Interrupt (unused) |

## Physical Specifications

| Parameter           | Value              |
|---------------------|--------------------|
| Dimensions          | 96mm x 92mm x 19mm |
| Weight              | ~118g              |
| Operating Temp      | -10°C to +50°C     |
| Operating Humidity  | 0% to 90%          |
| Power Supply        | 5V DC, 1A          |

## Use Cases
- Air quality monitor
- Smart home assistant / dashboard
- Weather station
- IoT sensor hub with visualization
- BLE gateway with display
- Digital signage
- Stock/crypto indicator

## References
- [Official Product Page](https://www.seeedstudio.com/SenseCAP-Indicator-D1-p-5643.html)
- [User Manual PDF](https://files.seeedstudio.com/products/SenseCAP/SenseCAP_Indicator/SenseCAP%20Indicator%20User%20Manual_2023.4.21.pdf)
- [Seeed Studio Wiki](https://wiki.seeedstudio.com/Sensor/SenseCAP/SenseCAP_Indicator/Get_started_with_SenseCAP_Indicator/)
- [GitHub SDK/Examples](https://github.com/Seeed-Solution/sensecap_indicator_esp32)
- [ESPHome Device Page](https://devices.esphome.io/devices/seeed-sensecap/)
- [Tasmota Pin Mapping](https://tasmota.github.io/docs/devices/SeedStudio-SenseCAP-D1/)
