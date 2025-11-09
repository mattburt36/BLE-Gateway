# Remote Config Server Setup Guide

## Overview

This guide explains how to set up a remote configuration server for BLE Gateway devices. The server provides device-specific configuration, MQTT credentials, and OTA firmware updates.

## Architecture

```
┌─────────────────┐
│  BLE Gateway    │
│  (XIAO ESP32)   │
└────────┬────────┘
         │
         ├─── HTTPS ───► gwconfig.hoptech.co.nz (Config Server)
         │                     │
         │                     ├─► Returns: MQTT credentials
         │                     ├─► Returns: Firmware URL
         │                     └─► Returns: Device config
         │
         └─── MQTTS ───► mqtt.hoptech.co.nz (RabbitMQ)
                              │
                              └─► ThingsBoard (Home)
                                      │
                                      └─► OTA firmware updates
```

## Option 1: Ubuntu Server (Recommended)

Host the config server on your Ubuntu server alongside RabbitMQ.

### Prerequisites

- Ubuntu server with RabbitMQ installed
- Python 3.8+ or Node.js 14+
- Nginx for HTTPS
- Domain pointing to server: `gwconfig.hoptech.co.nz`

---

## Python Flask Implementation

### 1. Install Dependencies

```bash
ssh user@your-ubuntu-server

# Install Python and dependencies
sudo apt update
sudo apt install python3-pip python3-venv nginx certbot python3-certbot-nginx -y

# Create application directory
sudo mkdir -p /opt/ble-gateway-config
sudo chown $USER:$USER /opt/ble-gateway-config
cd /opt/ble-gateway-config

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install Flask and dependencies
pip install flask pyyaml gunicorn hashlib
```

### 2. Create Config Server Application

Create `/opt/ble-gateway-config/app.py`:

```python
#!/usr/bin/env python3
"""
BLE Gateway Configuration Server
Provides device-specific configuration and MQTT credentials
"""

from flask import Flask, request, jsonify
import hashlib
import time
import os
import json
import yaml

app = Flask(__name__)

# Configuration
SECRET_SALT = os.environ.get('DEVICE_SALT', 'YOUR_SECRET_SALT_HERE_CHANGE_THIS')
MQTT_HOST = os.environ.get('MQTT_HOST', 'mqtt.hoptech.co.nz')
MQTT_PORT = int(os.environ.get('MQTT_PORT', '8883'))  # MQTTS port
COMPANY = os.environ.get('COMPANY', 'Hoptech')
DEVELOPMENT = os.environ.get('DEVELOPMENT', 'production')

# Device database (in production, use a real database)
DEVICES_FILE = '/opt/ble-gateway-config/devices.json'

def load_devices():
    """Load device database"""
    if os.path.exists(DEVICES_FILE):
        with open(DEVICES_FILE, 'r') as f:
            return json.load(f)
    return {}

def save_devices(devices):
    """Save device database"""
    with open(DEVICES_FILE, 'w') as f:
        json.dump(devices, f, indent=2)

def generate_device_password(device_id):
    """
    Generate MQTT password for device using device_id and secret salt
    This is the same hash that will be provisioned to the device
    """
    hash_input = f"{device_id}{SECRET_SALT}".encode('utf-8')
    return hashlib.sha256(hash_input).hexdigest()

def register_device(device_id):
    """Register a new device and return credentials"""
    devices = load_devices()
    
    if device_id not in devices:
        password = generate_device_password(device_id)
        
        devices[device_id] = {
            'device_id': device_id,
            'mqtt_user': device_id,
            'mqtt_password': password,
            'registered_at': time.time(),
            'last_seen': None,
            'firmware_version': None
        }
        
        save_devices(devices)
        print(f"New device registered: {device_id}")
    
    return devices[device_id]

@app.route('/<device_id>', methods=['GET'])
def get_device_config(device_id):
    """
    Get configuration for a specific device
    
    URL: http://gwconfig.hoptech.co.nz/{device_id}
    Headers:
      X-Device-ID: {device_id} (optional, for verification)
      X-Firmware-Version: {version} (optional, for tracking)
    """
    
    # Validate device_id format (12 hex characters)
    if len(device_id) != 12 or not all(c in '0123456789ABCDEFabcdef' for c in device_id):
        return jsonify({'error': 'Invalid device ID format'}), 400
    
    device_id = device_id.upper()
    
    # Get headers
    firmware_version = request.headers.get('X-Firmware-Version', 'unknown')
    
    # Register or update device
    device = register_device(device_id)
    
    # Update last seen and firmware version
    devices = load_devices()
    devices[device_id]['last_seen'] = time.time()
    devices[device_id]['firmware_version'] = firmware_version
    save_devices(devices)
    
    # Build configuration response
    config = {
        'device_id': device_id,
        'company': COMPANY,
        'development': DEVELOPMENT,
        'timestamp': int(time.time()),
        
        # MQTT Configuration
        'mqtt_host': MQTT_HOST,
        'mqtt_port': MQTT_PORT,
        'mqtt_user': device['mqtt_user'],
        'mqtt_password': device['mqtt_password'],
        'mqtt_use_tls': True,
        
        # OTA Firmware (if available)
        'firmware': {
            'version': '2.0.0',
            'url': f'https://firmware.hoptech.co.nz/ble-gateway-v2.0.0.bin',
            'size': 920000,
            'checksum': ''  # Optional SHA256 checksum
        }
    }
    
    return jsonify(config)

@app.route('/provision/<device_id>', methods=['POST'])
def provision_device(device_id):
    """
    Provision a new device (for manufacturing/setup)
    
    POST /provision/{device_id}
    Authorization: Bearer {admin_token}
    
    Returns the credentials that should be flashed to device
    """
    
    # Check admin token
    auth_header = request.headers.get('Authorization', '')
    admin_token = os.environ.get('ADMIN_TOKEN', 'change_this_admin_token')
    
    if auth_header != f'Bearer {admin_token}':
        return jsonify({'error': 'Unauthorized'}), 401
    
    device_id = device_id.upper()
    
    # Validate device_id
    if len(device_id) != 12:
        return jsonify({'error': 'Invalid device ID'}), 400
    
    # Generate credentials
    device = register_device(device_id)
    
    # Return credentials for provisioning
    return jsonify({
        'device_id': device_id,
        'mqtt_user': device['mqtt_user'],
        'mqtt_password': device['mqtt_password'],
        'mqtt_host': MQTT_HOST,
        'mqtt_port': MQTT_PORT,
        'provision_date': time.strftime('%Y-%m-%d %H:%M:%S')
    })

@app.route('/devices', methods=['GET'])
def list_devices():
    """
    List all registered devices
    
    GET /devices
    Authorization: Bearer {admin_token}
    """
    
    auth_header = request.headers.get('Authorization', '')
    admin_token = os.environ.get('ADMIN_TOKEN', 'change_this_admin_token')
    
    if auth_header != f'Bearer {admin_token}':
        return jsonify({'error': 'Unauthorized'}), 401
    
    devices = load_devices()
    
    # Format for display
    device_list = []
    for device_id, info in devices.items():
        device_list.append({
            'device_id': device_id,
            'firmware_version': info.get('firmware_version', 'unknown'),
            'last_seen': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(info['last_seen'])) if info.get('last_seen') else 'never',
            'registered_at': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(info['registered_at']))
        })
    
    return jsonify({'devices': device_list, 'total': len(device_list)})

@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint"""
    return jsonify({
        'status': 'healthy',
        'timestamp': int(time.time()),
        'service': 'ble-gateway-config'
    })

if __name__ == '__main__':
    # Development server
    app.run(host='0.0.0.0', port=5000, debug=True)
```

### 3. Create Environment File

Create `/opt/ble-gateway-config/.env`:

```bash
# IMPORTANT: Change these values!
DEVICE_SALT=your_secret_salt_here_minimum_32_chars_random
ADMIN_TOKEN=your_admin_token_here_minimum_32_chars_random

# MQTT Configuration
MQTT_HOST=mqtt.hoptech.co.nz
MQTT_PORT=8883

# Company/Environment
COMPANY=Hoptech
DEVELOPMENT=production

# Flask
FLASK_ENV=production
```

**Generate secure values:**
```bash
# Generate secret salt
openssl rand -hex 32

# Generate admin token
openssl rand -hex 32
```

### 4. Create Systemd Service

Create `/etc/systemd/system/ble-gateway-config.service`:

```ini
[Unit]
Description=BLE Gateway Configuration Server
After=network.target

[Service]
Type=notify
User=www-data
Group=www-data
WorkingDirectory=/opt/ble-gateway-config
Environment="PATH=/opt/ble-gateway-config/venv/bin"
EnvironmentFile=/opt/ble-gateway-config/.env
ExecStart=/opt/ble-gateway-config/venv/bin/gunicorn \
    --workers 2 \
    --bind unix:/opt/ble-gateway-config/app.sock \
    --access-logfile /var/log/ble-gateway-config/access.log \
    --error-logfile /var/log/ble-gateway-config/error.log \
    app:app

Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

### 5. Set Up Nginx

Create `/etc/nginx/sites-available/gwconfig`:

```nginx
server {
    listen 80;
    server_name gwconfig.hoptech.co.nz;

    # Redirect to HTTPS
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name gwconfig.hoptech.co.nz;

    # SSL Configuration (will be managed by certbot)
    ssl_certificate /etc/letsencrypt/live/gwconfig.hoptech.co.nz/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/gwconfig.hoptech.co.nz/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;

    access_log /var/log/nginx/gwconfig-access.log;
    error_log /var/log/nginx/gwconfig-error.log;

    location / {
        proxy_pass http://unix:/opt/ble-gateway-config/app.sock;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

Enable site:
```bash
sudo ln -s /etc/nginx/sites-available/gwconfig /etc/nginx/sites-enabled/
sudo nginx -t
```

### 6. Set Up SSL Certificate

```bash
# Install certificate (Let's Encrypt)
sudo certbot --nginx -d gwconfig.hoptech.co.nz

# Test auto-renewal
sudo certbot renew --dry-run
```

### 7. Create Log Directories

```bash
sudo mkdir -p /var/log/ble-gateway-config
sudo chown www-data:www-data /var/log/ble-gateway-config
```

### 8. Set Permissions

```bash
sudo chown -R www-data:www-data /opt/ble-gateway-config
sudo chmod 600 /opt/ble-gateway-config/.env
```

### 9. Start Services

```bash
# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable ble-gateway-config
sudo systemctl start ble-gateway-config

# Check status
sudo systemctl status ble-gateway-config

# Restart nginx
sudo systemctl restart nginx
```

---

## Configure RabbitMQ for MQTTS

### 1. Install RabbitMQ MQTT Plugin

```bash
# Enable MQTT plugin
sudo rabbitmq-plugins enable rabbitmq_mqtt

# Enable MQTT with TLS
sudo rabbitmq-plugins enable rabbitmq_mqtt rabbitmq_web_mqtt
```

### 2. Configure RabbitMQ

Edit `/etc/rabbitmq/rabbitmq.conf`:

```conf
# MQTT Configuration
mqtt.listeners.tcp.default = 1883
mqtt.listeners.ssl.default = 8883

# SSL Configuration for MQTT
mqtt.ssl_options.cacertfile = /etc/ssl/certs/ca-certificates.crt
mqtt.ssl_options.certfile = /etc/letsencrypt/live/mqtt.hoptech.co.nz/fullchain.pem
mqtt.ssl_options.keyfile = /etc/letsencrypt/live/mqtt.hoptech.co.nz/privkey.pem
mqtt.ssl_options.verify = verify_peer
mqtt.ssl_options.fail_if_no_peer_cert = false

# MQTT Settings
mqtt.allow_anonymous = false
mqtt.default_user = guest
mqtt.default_pass = guest
mqtt.vhost = /
mqtt.exchange = amq.topic
mqtt.subscription_ttl = 86400000
mqtt.prefetch = 10
```

### 3. Create MQTT User Script

Create `/opt/ble-gateway-config/create_mqtt_user.sh`:

```bash
#!/bin/bash
# Create RabbitMQ user for device

if [ $# -ne 2 ]; then
    echo "Usage: $0 <username> <password>"
    exit 1
fi

USERNAME=$1
PASSWORD=$2

# Create user
sudo rabbitmqctl add_user "$USERNAME" "$PASSWORD"

# Set permissions
sudo rabbitmqctl set_permissions -p / "$USERNAME" ".*" ".*" ".*"

# Set tags (optional)
sudo rabbitmqctl set_user_tags "$USERNAME" "device"

echo "User created: $USERNAME"
```

Make executable:
```bash
chmod +x /opt/ble-gateway-config/create_mqtt_user.sh
```

### 4. Auto-Create MQTT Users

Add to `/opt/ble-gateway-config/app.py`:

```python
import subprocess

def create_rabbitmq_user(username, password):
    """Create RabbitMQ user for device"""
    try:
        subprocess.run([
            '/opt/ble-gateway-config/create_mqtt_user.sh',
            username,
            password
        ], check=True, capture_output=True)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error creating RabbitMQ user: {e}")
        return False

# Call in register_device() function:
def register_device(device_id):
    devices = load_devices()
    
    if device_id not in devices:
        password = generate_device_password(device_id)
        
        devices[device_id] = {
            'device_id': device_id,
            'mqtt_user': device_id,
            'mqtt_password': password,
            'registered_at': time.time(),
            'last_seen': None,
            'firmware_version': None
        }
        
        save_devices(devices)
        
        # Create RabbitMQ user
        create_rabbitmq_user(device_id, password)
        
        print(f"New device registered: {device_id}")
    
    return devices[device_id]
```

### 5. Get SSL Certificate for MQTT

```bash
# Get certificate for mqtt.hoptech.co.nz
sudo certbot certonly --standalone -d mqtt.hoptech.co.nz

# Or if using nginx
sudo certbot --nginx -d mqtt.hoptech.co.nz
```

### 6. Restart RabbitMQ

```bash
sudo systemctl restart rabbitmq-server
sudo systemctl status rabbitmq-server
```

### 7. Test MQTTS Connection

```bash
# Install mosquitto clients
sudo apt install mosquitto-clients

# Test MQTTS connection
mosquitto_pub -h mqtt.hoptech.co.nz -p 8883 \
    -u testuser -P testpass \
    --capath /etc/ssl/certs/ \
    -t "test/topic" -m "hello" \
    --insecure  # Remove in production
```

---

## Device Provisioning Workflow

### 1. Get Device ID from ESP32

Flash the gateway code to get the device ID:
```
Device ID: A1B2C3D4E5F6
```

### 2. Provision Device via API

```bash
curl -X POST https://gwconfig.hoptech.co.nz/provision/A1B2C3D4E5F6 \
    -H "Authorization: Bearer YOUR_ADMIN_TOKEN"
```

Response:
```json
{
  "device_id": "A1B2C3D4E5F6",
  "mqtt_user": "A1B2C3D4E5F6",
  "mqtt_password": "hash_of_deviceid_with_salt",
  "mqtt_host": "mqtt.hoptech.co.nz",
  "mqtt_port": 8883,
  "provision_date": "2024-11-09 19:30:00"
}
```

### 3. Flash Credentials to Device

Update `src/main.cpp` to include provisioning mode or update `config_manager.h` to store encrypted credentials during first boot.

---

## Testing

### 1. Test Config Server

```bash
# Get device config
curl https://gwconfig.hoptech.co.nz/A1B2C3D4E5F6 \
    -H "X-Firmware-Version: 2.0.0"

# List devices (admin)
curl https://gwconfig.hoptech.co.nz/devices \
    -H "Authorization: Bearer YOUR_ADMIN_TOKEN"

# Health check
curl https://gwconfig.hoptech.co.nz/health
```

### 2. Test MQTTS

```bash
# Subscribe
mosquitto_sub -h mqtt.hoptech.co.nz -p 8883 \
    -u A1B2C3D4E5F6 -P hash_password \
    --capath /etc/ssl/certs/ \
    -t "gateway/A1B2C3D4E5F6/#" -v

# Publish
mosquitto_pub -h mqtt.hoptech.co.nz -p 8883 \
    -u A1B2C3D4E5F6 -P hash_password \
    --capath /etc/ssl/certs/ \
    -t "gateway/A1B2C3D4E5F6/status" \
    -m '{"test":"message"}'
```

---

## Monitoring

### View Logs

```bash
# Config server logs
sudo journalctl -u ble-gateway-config -f

# Nginx logs
sudo tail -f /var/log/nginx/gwconfig-access.log
sudo tail -f /var/log/nginx/gwconfig-error.log

# RabbitMQ logs
sudo tail -f /var/log/rabbitmq/rabbit@hostname.log
```

---

## Security Checklist

- [ ] Change `DEVICE_SALT` to a strong random value
- [ ] Change `ADMIN_TOKEN` to a strong random value
- [ ] Restrict `.env` file permissions (600)
- [ ] Enable SSL/TLS for config server (HTTPS)
- [ ] Enable SSL/TLS for MQTT (MQTTS on port 8883)
- [ ] Use Let's Encrypt certificates
- [ ] Disable anonymous MQTT access
- [ ] Set up firewall rules (allow 443, 8883)
- [ ] Regular certificate renewal (certbot auto-renew)
- [ ] Monitor failed authentication attempts

---

## Next Steps

1. Set up DNS records for `gwconfig.hoptech.co.nz` and `mqtt.hoptech.co.nz`
2. Deploy config server to Ubuntu server
3. Configure RabbitMQ with MQTTS
4. Update gateway code with MQTTS support
5. Test end-to-end flow
6. Document provisioning procedure
7. Set up monitoring/alerting

---

**Questions?** 
- How to handle firmware updates (hosting)?
- ThingsBoard integration specifics?
- Provisioning automation needs?
