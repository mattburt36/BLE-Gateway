# BLE Gateway Device Provisioning

## Overview

Each BLE Gateway device is provisioned with unique, secure credentials based on its MAC address. The credentials are generated using HMAC-SHA256 with a secret salt, ensuring:

- **Unique per device**: Each device has its own password
- **Deterministic**: Same device ID always generates same password
- **Secure**: Uses cryptographic hashing with a secret salt
- **Permanent**: Credentials are stored in encrypted NVS flash storage

## Provisioning Process

### 1. Generate Device Credentials

Use the provided Python script to generate credentials for a device:

```bash
python3 generate_device_password.py <DEVICE_ID>
```

Example:
```bash
python3 generate_device_password.py E86909E9AB4
```

This will output:
```
============================================================
BLE Gateway Device Credentials
============================================================
Device ID:  E86909E9AB4
Username:   ble-gateway-E86909E9AB4
Password:   pMffzIH9LEjWFDT9e+F1YCiRRX1ZNmql
============================================================

Provisioning Command:
PROVISION:ble-gateway-E86909E9AB4:pMffzIH9LEjWFDT9e+F1YCiRRX1ZNmql:
============================================================
```

### 2. Create RabbitMQ User

SSH to the RabbitMQ server (oracle) and create the user:

```bash
ssh oracle "sudo rabbitmqctl add_user 'ble-gateway-<DEVICE_ID>' '<PASSWORD>'"
ssh oracle "sudo rabbitmqctl set_permissions -p / 'ble-gateway-<DEVICE_ID>' '.*' '.*' '.*'"
ssh oracle "sudo rabbitmqctl set_user_tags 'ble-gateway-<DEVICE_ID>' management"
```

Example:
```bash
ssh oracle "sudo rabbitmqctl add_user 'ble-gateway-E86909E9AB4' 'pMffzIH9LEjWFDT9e+F1YCiRRX1ZNmql'"
ssh oracle "sudo rabbitmqctl set_permissions -p / 'ble-gateway-E86909E9AB4' '.*' '.*' '.*'"
ssh oracle "sudo rabbitmqctl set_user_tags 'ble-gateway-E86909E9AB4' management"
```

### 3. Provision the Device

Connect to the device via serial console and send the provisioning command:

```
PROVISION:<username>:<password>:
```

Example:
```
PROVISION:ble-gateway-E86909E9AB4:pMffzIH9LEjWFDT9e+F1YCiRRX1ZNmql:
```

The device will confirm:
```
✓ Credentials stored successfully!
  Username: ble-gateway-E86909E9AB4
  Password: ***ENCRYPTED***

[PROVISION] Device needs reboot to apply changes
```

### 4. Reboot the Device

Send the reboot command:
```
REBOOT
```

The device will restart and automatically connect to MQTT using the new credentials.

## Serial Commands

The device supports the following serial commands:

- `PROVISION:<user>:<pass>:<token>` - Store MQTT credentials
- `STATUS` - Show device status and configuration
- `CLEAR` - Clear all stored credentials
- `REBOOT` - Reboot the device
- `OTA:<url>` - Trigger OTA firmware update
- `HELP` - Show available commands

## Security Features

### Encrypted NVS Storage

All credentials are stored in ESP32's encrypted NVS (Non-Volatile Storage):
- Hardware-accelerated AES-256 encryption
- Unique encryption keys per device (stored in eFuse)
- Protected against firmware extraction attacks

### Password Generation

The password generation uses:
- **Algorithm**: HMAC-SHA256
- **Salt**: Secret salt (kept secure, not shared)
- **Input**: Device MAC address / Device ID
- **Output**: 32-character base64-encoded password

The secret salt ensures that:
- Only authorized personnel can generate valid passwords
- Passwords cannot be reverse-engineered from the device
- Different deployments can use different salts

## Device ID Format

The Device ID is derived from the device's MAC address:
```cpp
device_id = String(mac[0], HEX) + String(mac[1], HEX) + 
            String(mac[2], HEX) + String(mac[3], HEX) + 
            String(mac[4], HEX) + String(mac[5], HEX);
```

Example: MAC `E8:06:90:9E:9A:B4` → Device ID `E86909E9AB4`

## Verifying Connection

After provisioning and reboot, check the serial output for:

```
✅ ✅ ✅ MQTT CONNECTED SUCCESSFULLY! ✅ ✅ ✅
   Client ID: BLE-Gateway-E86909E9AB4
```

You can also send the `STATUS` command to verify:

```
========== DEVICE STATUS ==========
Device ID: E86909E9AB4
Firmware: 2.0.1
MQTT User: ble-gateway-E86909E9AB4
MQTT Password: ***SET***
MQTT Status: Connected
===================================
```

## Troubleshooting

### Device won't connect to MQTT

1. Verify credentials on RabbitMQ server:
   ```bash
   ssh oracle "sudo rabbitmqctl list_users"
   ```

2. Check device serial output for connection errors

3. Verify network connectivity from device

### Lost credentials

1. Generate new credentials using the script
2. Update RabbitMQ user password
3. Re-provision the device

### Clear and start over

Send `CLEAR` command followed by `REBOOT` to wipe all stored credentials and start fresh.

## Notes

- Keep the `SECRET_SALT` in `generate_device_password.py` confidential
- Back up generated credentials in a secure location
- Each device's credentials persist across firmware updates
- Credentials survive factory resets (stored in separate NVS partition)
