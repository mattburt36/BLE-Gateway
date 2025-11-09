# BLE Gateway - Quick Reference Card

## 🚀 Quick Start

```bash
# 1. Build firmware
cd BLE-Gateway
platformio run

# 2. Flash device
platformio run --target upload

# 3. Monitor serial
platformio device monitor

# 4. Get device ID from serial output
# Device ID: A1B2C3D4E5F6

# 5. Provision device
curl -X POST https://gwconfig.hoptech.co.nz/provision/A1B2C3D4E5F6 \
  -H "Authorization: Bearer YOUR_ADMIN_TOKEN"

# 6. Connect to AP and configure
# SSID: BLE-Gateway-Setup
# Password: 12345678
# Navigate to: http://192.168.4.1
```

---

## 📁 Project Structure

```
BLE-Gateway/
├── platformio.ini          XIAO ESP32-S3 config
├── build.sh               Build script
├── src/
│   ├── main.cpp          Main app
│   ├── config_manager.h   Flash storage
│   ├── wifi_manager.h     WiFi + NTP + remote config
│   ├── mqtt_handler.h     MQTTS connection
│   ├── ble_scanner.h      BLE scanning
│   ├── device_tracker.h   6h change detection
│   └── ota_manager.h      OTA updates
└── docs/
    ├── README_V2.md       Main guide
    ├── SERVER_SETUP.md    Server setup
    ├── PROVISIONING.md    Provisioning guide
    └── IMPLEMENTATION.md  Complete summary
```

---

## 🔧 Key Configuration

### Device Settings

| Setting | Value | Location |
|---------|-------|----------|
| Firmware Version | 2.0.0 | main.cpp |
| Change Threshold (Temp) | ±0.5°C | device_tracker.h |
| Change Threshold (Humidity) | ±2% | device_tracker.h |
| Change Threshold (Battery) | ±5% | device_tracker.h |
| Keepalive Interval | 6 hours | device_tracker.h |
| Device Expiry | 6 hours | device_tracker.h |
| BLE Scan Interval | 10 seconds | ble_scanner.h |
| BLE Scan Duration | 5 seconds | ble_scanner.h |
| MQTT Keepalive | 60 seconds | mqtt_handler.h |
| Gateway Status | 5 minutes | mqtt_handler.h |

### Server Settings

| Setting | Value |
|---------|-------|
| Config Server | https://gwconfig.hoptech.co.nz |
| MQTT Broker | mqtt.hoptech.co.nz |
| MQTT Port | 8883 (MQTTS) |
| Web Portal IP | 192.168.4.1 |
| Web Portal SSID | BLE-Gateway-Setup |
| Web Portal Password | 12345678 |

---

## 📡 MQTT Topics

### Publish (Device → Server)

```
gateway/{device_id}/status
  • Gateway health (every 5 min)
  • Fields: uptime, free_heap, wifi_rssi, firmware

gateway/{device_id}/device/{mac}
  • BLE sensor data (on change or 6h)
  • Fields: temperature, humidity, battery, rssi, changed

gateway/{device_id}/ota/status
  • OTA progress
  • States: downloading, updating, success, failed
```

### Subscribe (Server → Device)

```
gateway/{device_id}/command
  • Commands: restart, status

gateway/{device_id}/ota
  • OTA trigger
  • Payload: {version, url, size}
```

---

## 🔐 Security

### Password Generation
```python
import hashlib
hash_input = f"{device_id}{SECRET_SALT}"
password = hashlib.sha256(hash_input.encode()).hexdigest()
```

### Environment Variables (.env)
```bash
DEVICE_SALT=your_secret_salt_32_chars_minimum
ADMIN_TOKEN=your_admin_token_32_chars_minimum
MQTT_HOST=mqtt.hoptech.co.nz
MQTT_PORT=8883
```

---

## 📊 System Status

### Monitor Logs

```bash
# Config server
sudo journalctl -u ble-gateway-config -f

# Nginx
sudo tail -f /var/log/nginx/gwconfig-access.log

# RabbitMQ
sudo tail -f /var/log/rabbitmq/rabbit@hostname.log

# Device serial
platformio device monitor
```

### Health Checks

```bash
# Config server
curl https://gwconfig.hoptech.co.nz/health

# List devices
curl https://gwconfig.hoptech.co.nz/devices \
  -H "Authorization: Bearer YOUR_ADMIN_TOKEN"

# Test MQTTS
mosquitto_sub -h mqtt.hoptech.co.nz -p 8883 \
  -u DEVICEID -P PASSWORD \
  --capath /etc/ssl/certs/ \
  -t "gateway/#" -v
```

---

## 🛠️ Common Commands

### Build & Flash
```bash
platformio run                    # Build
platformio run --target upload    # Flash
platformio device monitor         # Monitor serial
platformio run --target clean     # Clean build
```

### Server Management
```bash
# Restart config server
sudo systemctl restart ble-gateway-config

# Restart RabbitMQ
sudo systemctl restart rabbitmq-server

# Restart Nginx
sudo systemctl restart nginx

# View status
sudo systemctl status ble-gateway-config
```

### Device Provisioning
```bash
# Provision new device
curl -X POST https://gwconfig.hoptech.co.nz/provision/A1B2C3D4E5F6 \
  -H "Authorization: Bearer YOUR_ADMIN_TOKEN"

# Create RabbitMQ user
/opt/ble-gateway-config/create_mqtt_user.sh DEVICEID PASSWORD
```

---

## 🐛 Troubleshooting

| Issue | Solution |
|-------|----------|
| Won't connect to MQTTS | Check credentials, verify RabbitMQ port 8883, test with mosquitto |
| OTA fails | Verify firmware URL accessible, check file size, monitor serial |
| Device not publishing | Check MQTT connection, verify thresholds, check change detection |
| Config server 500 error | Check logs, verify .env file, check permissions |
| SSL certificate error | Renew with certbot, check certificate paths |
| Device ID not found | Check serial monitor (115200 baud), wait for boot |

---

## 📞 Quick Commands Reference

```bash
# Generate secure tokens
openssl rand -hex 32

# Test HTTPS
curl -v https://gwconfig.hoptech.co.nz/health

# Test MQTTS publish
mosquitto_pub -h mqtt.hoptech.co.nz -p 8883 \
  -u DEVICEID -P PASSWORD \
  --capath /etc/ssl/certs/ \
  -t "test/topic" -m "hello"

# View certificate
openssl s_client -connect mqtt.hoptech.co.nz:8883 -showcerts

# Check open ports
sudo netstat -tlnp | grep -E '443|8883'

# Renew SSL certificates
sudo certbot renew

# Flash erase (if needed)
esptool.py --chip esp32s3 erase_flash
```

---

## 📝 Important Files

### Server
- `/opt/ble-gateway-config/app.py` - Config server
- `/opt/ble-gateway-config/.env` - Environment variables
- `/opt/ble-gateway-config/devices.json` - Device database
- `/etc/rabbitmq/rabbitmq.conf` - RabbitMQ config
- `/etc/nginx/sites-available/gwconfig` - Nginx config

### Device
- `src/main.cpp` - Main application
- `src/device_tracker.h` - Change detection logic
- `platformio.ini` - Build configuration

---

## 🎯 Next Steps Checklist

- [ ] Deploy config server (SERVER_SETUP.md)
- [ ] Configure RabbitMQ MQTTS
- [ ] Install SSL certificates
- [ ] Test provisioning API
- [ ] Flash test device
- [ ] Verify MQTT connection
- [ ] Test BLE scanning
- [ ] Test OTA update
- [ ] Integrate with ThingsBoard
- [ ] Production deployment

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| README_V2.md | Main documentation and feature overview |
| SERVER_SETUP.md | Complete server setup guide (Flask, RabbitMQ, SSL) |
| PROVISIONING.md | Device provisioning workflows and scripts |
| IMPLEMENTATION.md | Complete implementation summary |

---

**Version:** 2.0.0  
**Platform:** XIAO ESP32-S3  
**Updated:** November 2024
