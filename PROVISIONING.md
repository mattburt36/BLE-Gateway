# Device Provisioning Guide

## Overview

This guide covers the complete provisioning process for BLE Gateway devices, including credential generation, flash programming, and server registration.

---

## Provisioning Workflow

```
┌─────────────────────────────────────────────────────────────┐
│                  PROVISIONING WORKFLOW                      │
└─────────────────────────────────────────────────────────────┘

1. Flash Gateway Firmware (Generic)
        ↓
2. Device Boots → Generates Device ID from MAC
        ↓
3. Device Enters AP Mode (no credentials)
        ↓
4. Provisioner Connects to AP
        ↓
5. Provisioner Calls Server API → Get Credentials
        ↓
6. Provisioner Submits Credentials via Web Portal
        ↓
7. Device Stores Encrypted Credentials
        ↓
8. Device Reboots → Connects to Config Server
        ↓
9. Config Server Auto-Creates MQTT User
        ↓
10. Device Connects to MQTTS → Operational
```

---

## Method 1: Web Portal Provisioning (Recommended)

### Step 1: Flash Base Firmware

```bash
# Build firmware
cd BLE-Gateway
platformio run

# Flash to XIAO ESP32-S3
platformio run --target upload
```

### Step 2: Get Device ID

Monitor serial output:
```
========================================
BLE-Gateway-XIAO v2.0.0
XIAO ESP32-S3 BLE Gateway
========================================

Device ID: A1B2C3D4E5F6

No configuration found in flash.
Starting WiFi Access Point for configuration...

✓ Configuration portal started
  SSID: BLE-Gateway-Setup
  Password: 12345678
  IP: 192.168.4.1
```

**Note down the Device ID:** `A1B2C3D4E5F6`

### Step 3: Get Device Credentials from Server

```bash
# Call provision API
curl -X POST https://gwconfig.hoptech.co.nz/provision/A1B2C3D4E5F6 \
    -H "Authorization: Bearer YOUR_ADMIN_TOKEN" \
    | jq

# Response:
{
  "device_id": "A1B2C3D4E5F6",
  "mqtt_user": "A1B2C3D4E5F6",
  "mqtt_password": "d4f8e9a7b3c2...hash...",
  "mqtt_host": "mqtt.hoptech.co.nz",
  "mqtt_port": 8883,
  "provision_date": "2024-11-09 20:00:00"
}
```

### Step 4: Configure Device via Web Portal

1. Connect to WiFi: `BLE-Gateway-Setup` (password: `12345678`)
2. Navigate to: `http://192.168.4.1`
3. Enter configuration:
   - **WiFi SSID:** Your network SSID
   - **WiFi Password:** Your network password
   - **MQTT Host:** `mqtt.hoptech.co.nz`
   - **MQTT Username:** `A1B2C3D4E5F6` (device ID)
   - **MQTT Password:** `d4f8e9a7b3c2...` (from server response)
4. Click "Save & Restart"

### Step 5: Verify Operation

Monitor serial output:
```
✓ WiFi connected
  IP Address: 192.168.1.100

✓ Time synchronized: Sat Nov 9 20:05:30 2024

Fetching remote configuration from: http://gwconfig.hoptech.co.nz/A1B2C3D4E5F6
✓ Remote configuration retrieved:
  Company: Hoptech
  Development: production
  MQTT Host: mqtt.hoptech.co.nz

✓ MQTTS connected
  Client ID: BLE-Gateway-A1B2C3D4E5F6
  Subscribed to: gateway/A1B2C3D4E5F6/command
  Subscribed to: gateway/A1B2C3D4E5F6/ota

Creating FreeRTOS tasks...
All tasks created successfully!
```

---

## Method 2: Automated Provisioning Script

For production environments where you provision many devices.

### Create Provisioning Script

Create `provision_device.py`:

```python
#!/usr/bin/env python3
"""
Automated BLE Gateway Provisioning Script

Usage:
    python3 provision_device.py --device-id A1B2C3D4E5F6
    
Or auto-detect from serial:
    python3 provision_device.py --port /dev/ttyUSB0
"""

import requests
import serial
import time
import argparse
import re
import json

CONFIG_SERVER = "https://gwconfig.hoptech.co.nz"
ADMIN_TOKEN = "YOUR_ADMIN_TOKEN_HERE"
GATEWAY_AP_SSID = "BLE-Gateway-Setup"
GATEWAY_AP_PASSWORD = "12345678"
GATEWAY_AP_IP = "192.168.4.1"

def get_device_id_from_serial(port, baudrate=115200, timeout=30):
    """
    Read device ID from serial monitor
    """
    print(f"Connecting to {port}...")
    
    ser = serial.Serial(port, baudrate, timeout=1)
    
    start_time = time.time()
    device_id = None
    
    while time.time() - start_time < timeout:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            print(line)
            
            # Look for Device ID in output
            match = re.search(r'Device ID: ([0-9A-F]{12})', line)
            if match:
                device_id = match.group(1)
                break
    
    ser.close()
    
    if device_id:
        print(f"\n✓ Device ID found: {device_id}")
        return device_id
    else:
        print("\n✗ Could not find Device ID in serial output")
        return None

def provision_device(device_id):
    """
    Get credentials from config server
    """
    url = f"{CONFIG_SERVER}/provision/{device_id}"
    headers = {"Authorization": f"Bearer {ADMIN_TOKEN}"}
    
    print(f"\nProvisioning device {device_id}...")
    
    response = requests.post(url, headers=headers)
    
    if response.status_code == 200:
        data = response.json()
        print("\n✓ Device provisioned successfully:")
        print(json.dumps(data, indent=2))
        return data
    else:
        print(f"\n✗ Provisioning failed: {response.status_code}")
        print(response.text)
        return None

def configure_device_wifi(credentials, wifi_ssid, wifi_password):
    """
    Send credentials to device via web portal
    
    Note: You must be connected to the device's AP
    """
    url = f"http://{GATEWAY_AP_IP}/save"
    
    data = {
        'ssid': wifi_ssid,
        'password': wifi_password,
        'mqtt_host': credentials['mqtt_host'],
        'mqtt_user': credentials['mqtt_user'],
        'mqtt_pass': credentials['mqtt_password']
    }
    
    print(f"\nConfiguring device via web portal...")
    print(f"Make sure you're connected to: {GATEWAY_AP_SSID}")
    
    try:
        response = requests.post(url, data=data, timeout=5)
        if response.status_code == 200:
            print("✓ Configuration sent successfully")
            print("Device will restart and connect to WiFi")
            return True
        else:
            print(f"✗ Configuration failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Error sending configuration: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Provision BLE Gateway device')
    parser.add_argument('--device-id', help='Device ID (12 hex characters)')
    parser.add_argument('--port', help='Serial port to read device ID from')
    parser.add_argument('--wifi-ssid', required=True, help='WiFi SSID to configure')
    parser.add_argument('--wifi-password', required=True, help='WiFi password')
    
    args = parser.parse_args()
    
    # Get device ID
    device_id = args.device_id
    
    if not device_id and args.port:
        device_id = get_device_id_from_serial(args.port)
    
    if not device_id:
        print("Error: No device ID provided or detected")
        return 1
    
    # Provision device (get MQTT credentials)
    credentials = provision_device(device_id)
    
    if not credentials:
        return 1
    
    # Configure device
    input("\nPress Enter when connected to the device AP...")
    
    configure_device_wifi(credentials, args.wifi_ssid, args.wifi_password)
    
    print("\n" + "="*60)
    print("Provisioning complete!")
    print("="*60)
    print(f"\nDevice ID: {device_id}")
    print(f"MQTT User: {credentials['mqtt_user']}")
    print(f"MQTT Host: {credentials['mqtt_host']}")
    print("\nDevice should now connect to WiFi and MQTT automatically.")

if __name__ == '__main__':
    main()
```

### Install Dependencies

```bash
pip install pyserial requests
```

### Run Provisioning

```bash
# Auto-detect device ID from serial
python3 provision_device.py \
    --port /dev/ttyUSB0 \
    --wifi-ssid "YourWiFi" \
    --wifi-password "YourPassword"

# Or specify device ID directly
python3 provision_device.py \
    --device-id A1B2C3D4E5F6 \
    --wifi-ssid "YourWiFi" \
    --wifi-password "YourPassword"
```

---

## Method 3: Pre-Provisioned Credentials (Factory)

For manufacturing environments where you want to pre-flash credentials.

### Step 1: Modify config_manager.h

Add factory provisioning support:

```cpp
void provisionFactory(const String& deviceId, const String& mqttPassword) {
    preferences.putString("mqtt_user", deviceId);
    preferences.putString("mqtt_pass", mqttPassword);
    preferences.putBool("provisioned", true);
    
    Serial.println("✓ Factory provisioned");
}

bool isProvisioned() {
    return preferences.getBool("provisioned", false);
}
```

### Step 2: Create Provisioning Firmware

Create temporary provisioning code in main.cpp:

```cpp
// FACTORY PROVISIONING MODE
#define FACTORY_PROVISION true
#define PROVISION_DEVICE_ID "A1B2C3D4E5F6"
#define PROVISION_MQTT_PASSWORD "hash_from_server"

void setup() {
    // ... normal setup code ...
    
    #if FACTORY_PROVISION
    Serial.println("\n=== FACTORY PROVISIONING MODE ===");
    provisionFactory(PROVISION_DEVICE_ID, PROVISION_MQTT_PASSWORD);
    Serial.println("Device provisioned. Remove FACTORY_PROVISION and reflash.");
    while(1) { delay(1000); }
    #endif
    
    // ... rest of setup ...
}
```

### Step 3: Flash Provisioning Firmware

1. Get credentials from server
2. Update `PROVISION_DEVICE_ID` and `PROVISION_MQTT_PASSWORD`
3. Set `FACTORY_PROVISION true`
4. Build and flash
5. Verify provisioning on serial monitor
6. Set `FACTORY_PROVISION false`
7. Build and flash final firmware

---

## Credential Security

### Password Hashing Algorithm

The server generates MQTT passwords using:
```python
hash_input = f"{device_id}{SECRET_SALT}"
mqtt_password = hashlib.sha256(hash_input.encode()).hexdigest()
```

**Important:**
- `SECRET_SALT` must be kept secret
- Same salt used for all devices
- Salt minimum 32 characters
- Passwords are 64-character hex strings (SHA256)

### Storage Encryption

Credentials are stored in ESP32 flash using the Preferences API:
- Stored in encrypted NVS (if flash encryption enabled)
- Namespace: `gateway`
- Keys: `wifi_ssid`, `wifi_pass`, `mqtt_host`, `mqtt_user`, `mqtt_pass`

### Enable Flash Encryption (Optional)

For production, enable ESP32 flash encryption:

```ini
; platformio.ini
build_flags = 
    -DCONFIG_SECURE_FLASH_ENC_ENABLED=1
```

Then use `esptool.py` to enable flash encryption (one-time, irreversible).

---

## Batch Provisioning

For provisioning multiple devices:

### Create Batch Script

```bash
#!/bin/bash
# batch_provision.sh

WIFI_SSID="YourWiFi"
WIFI_PASSWORD="YourPassword"

DEVICES=(
    "A1B2C3D4E5F6"
    "B2C3D4E5F6A1"
    "C3D4E5F6A1B2"
)

for device_id in "${DEVICES[@]}"; do
    echo "Provisioning $device_id..."
    
    python3 provision_device.py \
        --device-id "$device_id" \
        --wifi-ssid "$WIFI_SSID" \
        --wifi-password "$WIFI_PASSWORD"
    
    echo "Waiting for next device..."
    read -p "Connect next device and press Enter..."
done

echo "Batch provisioning complete!"
```

---

## Troubleshooting

### Device ID Not Found
- Check serial connection (115200 baud)
- Ensure device is powered and booting
- Wait for full boot sequence

### Provisioning API Fails
- Check admin token
- Verify config server is running
- Check network connectivity
- Review server logs

### MQTT Connection Fails
- Verify credentials match server
- Check RabbitMQ is running and accepting connections on port 8883
- Test with mosquitto_pub/sub
- Check firewall rules

### Configuration Not Saving
- Check flash is not write-protected
- Monitor serial for error messages
- Verify Preferences API is working

---

## Production Checklist

- [ ] Config server deployed and accessible
- [ ] SSL certificates installed and valid
- [ ] RabbitMQ configured for MQTTS (port 8883)
- [ ] Admin token generated and secured
- [ ] Secret salt generated and secured
- [ ] Provisioning script tested
- [ ] Test device provisioned successfully
- [ ] MQTT connection verified
- [ ] OTA update tested
- [ ] Documentation updated with actual URLs/credentials

---

## Next Steps

1. Test provisioning with one device
2. Verify MQTT connectivity
3. Test OTA update
4. Document any environment-specific steps
5. Create provisioning checklist for factory/field use

