# SenseCAP Indicator D1 - Complete Pinout Reference

## Display System Pin Mapping

### ST7701S Display Controller (RGB 8-bit Parallel Interface)

#### RGB Data Lines
The display uses an 8-bit RGB parallel interface with the following pin mapping:

**Red Channel (5 bits)**
| Bit | GPIO  | Description |
|-----|-------|-------------|
| R0  | GPIO0 | Red LSB     |
| R1  | GPIO1 |             |
| R2  | GPIO2 |             |
| R3  | GPIO3 |             |
| R4  | GPIO4 | Red MSB     |

**Green Channel (6 bits)**
| Bit | GPIO   | Description |
|-----|--------|-------------|
| G0  | GPIO5  | Green LSB   |
| G1  | GPIO6  |             |
| G2  | GPIO7  |             |
| G3  | GPIO8  |             |
| G4  | GPIO9  |             |
| G5  | GPIO10 | Green MSB   |

**Blue Channel (5 bits)**
| Bit | GPIO   | Description |
|-----|--------|-------------|
| B0  | GPIO11 | Blue LSB    |
| B1  | GPIO12 |             |
| B2  | GPIO13 |             |
| B3  | GPIO14 |             |
| B4  | GPIO15 | Blue MSB    |

#### Display Control Signals
| Signal    | GPIO/Pin  | Description                    |
|-----------|-----------|--------------------------------|
| HSYNC     | GPIO16    | Horizontal Sync                |
| VSYNC     | GPIO17    | Vertical Sync                  |
| DE        | GPIO18    | Data Enable                    |
| PCLK      | GPIO21    | Pixel Clock                    |
| DISP_EN   | Expander/5| Display Enable (PCA9535 bit 5) |
| LCD_RST   | Expander/4| Display Reset (PCA9535 bit 4)  |

### Touch Controller (FT5x06)

The touch controller uses I2C interface:

| Signal | GPIO   | I2C Address | Description        |
|--------|--------|-------------|--------------------|
| SDA    | GPIO39 | 0x38        | Touch Data         |
| SCL    | GPIO40 |             | Touch Clock        |
| INT    | GPIO42 |             | Touch Interrupt    |
| RST    | Expander/6 |          | Touch Reset (PCA9535 bit 6) |

## Communication Interfaces

### SPI Bus
Used for LoRa module and other peripherals:

| Signal | GPIO   | Alt Function | Description              |
|--------|--------|--------------|--------------------------|
| SCK    | GPIO41 | SPI_CLK      | SPI Clock                |
| MOSI   | GPIO48 | SPI_MOSI     | Master Out Slave In      |
| MISO   | GPIO47 | SPI_MISO     | Master In Slave Out      |
| CS     | GPIO45 | SPI_CS       | Chip Select (LoRa/other) |

### I2C Bus
Primary I2C bus for sensors and peripherals:

| Signal | GPIO   | Devices Connected           |
|--------|--------|-----------------------------|
| SDA    | GPIO39 | Touch, PCA9535, Grove       |
| SCL    | GPIO40 | Touch, PCA9535, Grove       |

**I2C Device Addresses:**
- **PCA9535 I/O Expander**: 0x20
- **FT5x06 Touch**: 0x38
- **Grove Sensors**: Various (check sensor datasheet)

## I/O Expander (PCA9535)

The PCA9535 provides 16 additional GPIO pins:

### Port 0 (Bits 0-7)
| Bit | Function      | Direction | Description                |
|-----|---------------|-----------|----------------------------|
| 0   | USER_LED_R    | Output    | Red LED                    |
| 1   | USER_LED_G    | Output    | Green LED                  |
| 2   | USER_LED_B    | Output    | Blue LED                   |
| 3   | GROVE_PWR     | Output    | Grove Port Power Enable    |
| 4   | LCD_RST       | Output    | Display Reset              |
| 5   | DISP_EN       | Output    | Display Enable             |
| 6   | TP_RST        | Output    | Touch Panel Reset          |
| 7   | RESERVED      | -         | Reserved                   |

### Port 1 (Bits 8-15)
| Bit | Function      | Direction | Description                |
|-----|---------------|-----------|----------------------------|
| 8   | GROVE_SDA_EN  | Output    | Grove I2C Pull-up Enable   |
| 9   | GROVE_SCL_EN  | Output    | Grove I2C Pull-up Enable   |
| 10  | RP2040_RST    | Output    | RP2040 Reset               |
| 11  | RP2040_BOOT   | Output    | RP2040 Boot Mode           |
| 12  | BUZZER_EN     | Output    | Buzzer Enable              |
| 13  | SD_PWR        | Output    | SD Card Power              |
| 14  | BUTTON_1      | Input     | User Button 1              |
| 15  | BUTTON_2      | Input     | User Button 2              |

## Grove Expansion Ports

### Grove Port 1 (Left)
| Pin | Signal    | GPIO   | Function              |
|-----|-----------|--------|-----------------------|
| 1   | GND       | -      | Ground                |
| 2   | VCC       | -      | 3.3V or 5V (jumper)   |
| 3   | SDA/D0    | GPIO39 | I2C Data / Digital    |
| 4   | SCL/D1    | GPIO40 | I2C Clock / Digital   |

### Grove Port 2 (Right)
| Pin | Signal    | GPIO   | Function              |
|-----|-----------|--------|-----------------------|
| 1   | GND       | -      | Ground                |
| 2   | VCC       | -      | 3.3V or 5V (jumper)   |
| 3   | A0/D0     | GPIO1  | Analog / Digital      |
| 4   | A1/D1     | GPIO2  | Analog / Digital      |

**Note:** GPIO1 and GPIO2 are shared with Red display data lines. Ensure display is not active when using Grove Port 2 for I/O.

## RP2040 Communication

### UART Interface (ESP32-S3 ↔ RP2040)
| Signal | ESP32-S3 GPIO | RP2040 Pin | Description      |
|--------|---------------|------------|------------------|
| TX     | GPIO43        | GPIO0 (RX) | ESP32 → RP2040   |
| RX     | GPIO44        | GPIO1 (TX) | RP2040 → ESP32   |

**Protocol:** COBS (Consistent Overhead Byte Stuffing) framing

## SD Card Interface

Connected to RP2040 via SPI:

| Signal | RP2040 GPIO | Function           |
|--------|-------------|--------------------|
| CS     | GPIO17      | Chip Select        |
| MOSI   | GPIO19      | Master Out         |
| MISO   | GPIO16      | Master In          |
| SCK    | GPIO18      | Clock              |
| CD     | GPIO13      | Card Detect        |

## USB Interfaces

### USB-C Port 1 (ESP32-S3)
- **D+**: GPIO19
- **D-**: GPIO20
- **Use**: Programming, Serial, USB OTG

### USB-C Port 2 (RP2040)
- **D+**: Internal
- **D-**: Internal
- **Use**: Programming, Serial, Mass Storage

## LoRa Module (Optional)

Semtech SX1262 connected via SPI:

| Signal | GPIO   | Function           |
|--------|--------|--------------------|
| SCK    | GPIO41 | SPI Clock          |
| MOSI   | GPIO48 | SPI MOSI           |
| MISO   | GPIO47 | SPI MISO           |
| CS     | GPIO45 | Chip Select        |
| RST    | GPIO46 | Reset              |
| BUSY   | GPIO38 | Busy Signal        |
| DIO1   | GPIO33 | Interrupt          |

## Buzzer

| Signal | Control       | Frequency | Description    |
|--------|---------------|-----------|----------------|
| PWM    | Expander/12   | 2700Hz    | MLT-8530       |

Controlled via PCA9535 bit 12 with PWM from RP2040.

## Power Management

| Rail  | Voltage | Current | Controlled By    |
|-------|---------|---------|------------------|
| VCC   | 5V      | 1A      | USB-C Input      |
| 3V3   | 3.3V    | 800mA   | LDO Regulator    |
| GROVE | 3.3V/5V | 500mA   | PCA9535 Switch   |
| LCD   | 3.3V    | 200mA   | PCA9535 Enable   |
| SD    | 3.3V    | 100mA   | PCA9535 Enable   |

## Pin Usage Summary

### Available GPIOs (ESP32-S3)
After display and core functions, the following GPIOs are available:

| GPIO   | Function          | Available? | Notes                    |
|--------|-------------------|------------|--------------------------|
| GPIO1  | Grove/Display     | Shared     | Red data or Grove A0     |
| GPIO2  | Grove/Display     | Shared     | Red data or Grove A1     |
| GPIO33 | LoRa DIO1         | Conditional| Free if no LoRa module   |
| GPIO38 | LoRa BUSY         | Conditional| Free if no LoRa module   |
| GPIO43 | UART TX           | Used       | RP2040 communication     |
| GPIO44 | UART RX           | Used       | RP2040 communication     |
| GPIO46 | LoRa RST          | Conditional| Free if no LoRa module   |

**Note:** Most GPIOs are dedicated to display or core functions. Use Grove ports or RP2040 for additional I/O.

## Recommendations for BLE Gateway Project

### Pins to Use:
- **BLE Scanning**: No physical pins required (uses ESP32-S3 radio)
- **Display**: Already mapped above
- **WiFi/MQTT**: No physical pins required (uses ESP32-S3 radio)
- **Status LED**: Use PCA9535 bits 0-2 (RGB LED)
- **User Input**: Use PCA9535 bits 14-15 (Buttons)
- **Additional Sensors**: Use Grove ports

### Pins to Avoid:
- GPIO0-21: Reserved for display
- GPIO39-40: Reserved for I2C (touch + sensors)
- GPIO41, 47-48: Reserved for SPI
- GPIO43-44: Reserved for RP2040 UART

## References
- [ESPHome Device Configuration](https://devices.esphome.io/devices/seeed-sensecap/)
- [Tasmota Pin Template](https://tasmota.github.io/docs/devices/SeedStudio-SenseCAP-D1/)
- [Official Schematics (in User Manual)](https://files.seeedstudio.com/products/SenseCAP/SenseCAP_Indicator/SenseCAP%20Indicator%20User%20Manual_2023.4.21.pdf)
