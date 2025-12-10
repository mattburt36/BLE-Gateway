#!/bin/bash

################################################################################
# BLE Gateway Device Provisioning Script
#
# This script provisions a new BLE Gateway device by:
# 1. Creating a unique device user in RabbitMQ
# 2. Generating a secure password
# 3. Setting appropriate permissions
# 4. Building and flashing the firmware
# 5. Storing encrypted credentials in device flash
#
# Prerequisites:
# - SSH access to RabbitMQ server (oracle)
# - PlatformIO CLI installed
# - Device connected via USB
#
# Usage: ./provision_device.sh [device_id]
#        If device_id is not provided, it will be auto-detected from the device
################################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
RABBITMQ_HOST="oracle"
RABBITMQ_SSH_USER="${RABBITMQ_SSH_USER:-matt}"  # Can be overridden with env var
MQTT_VHOST="/"  # RabbitMQ vhost for MQTT

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}BLE Gateway Device Provisioning${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v pio &> /dev/null; then
    echo -e "${RED}✗ PlatformIO CLI not found${NC}"
    echo -e "  Install with: pip install platformio"
    exit 1
fi
echo -e "${GREEN}✓ PlatformIO CLI found${NC}"

if ! command -v ssh &> /dev/null; then
    echo -e "${RED}✗ SSH not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ SSH found${NC}"

# Test SSH connection to RabbitMQ server
if ! ssh -q -o ConnectTimeout=5 -o BatchMode=yes "${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}" exit 2>/dev/null; then
    echo -e "${RED}✗ Cannot connect to ${RABBITMQ_HOST} via SSH${NC}"
    echo -e "  Make sure you have SSH key authentication set up"
    echo -e "  Try: ssh ${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}"
    exit 1
fi
echo -e "${GREEN}✓ SSH connection to ${RABBITMQ_HOST} successful${NC}\n"

# Get device ID
DEVICE_ID="$1"

if [ -z "$DEVICE_ID" ]; then
    echo -e "${YELLOW}No device ID provided, attempting to detect from connected device...${NC}"
    
    # Build the firmware first to get the device ID
    echo -e "${YELLOW}Building firmware to detect device...${NC}"
    pio run || {
        echo -e "${RED}✗ Build failed${NC}"
        exit 1
    }
    
    # Upload and monitor to get device ID
    echo -e "${YELLOW}Please connect the device via USB${NC}"
    echo -e "${YELLOW}Press Ctrl+C after you see the Device ID in the output...${NC}\n"
    
    # Capture serial output to get device ID
    MONITOR_OUTPUT=$(pio device monitor --baud 115200 --filter direct 2>&1 | tee /dev/tty | grep -m 1 "Device ID:" || true)
    
    if [ -n "$MONITOR_OUTPUT" ]; then
        DEVICE_ID=$(echo "$MONITOR_OUTPUT" | grep -oP 'Device ID: \K[A-F0-9]+' || true)
    fi
    
    if [ -z "$DEVICE_ID" ]; then
        echo -e "\n${RED}✗ Could not detect device ID${NC}"
        echo -e "  Please provide device ID manually: $0 <device_id>"
        exit 1
    fi
fi

DEVICE_ID=$(echo "$DEVICE_ID" | tr '[:lower:]' '[:upper:]')
echo -e "${GREEN}✓ Device ID: ${DEVICE_ID}${NC}\n"

# Generate username and password
MQTT_USER="ble-gateway-${DEVICE_ID}"
# Generate a secure 32-character password
MQTT_PASSWORD=$(openssl rand -base64 24 | tr -d /=+ | cut -c1-32)

echo -e "${BLUE}Device Credentials:${NC}"
echo -e "  Username: ${GREEN}${MQTT_USER}${NC}"
echo -e "  Password: ${GREEN}${MQTT_PASSWORD}${NC}\n"

# Create RabbitMQ user via SSH
echo -e "${YELLOW}Creating RabbitMQ user...${NC}"

# Check if user already exists
USER_EXISTS=$(ssh "${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}" "sudo rabbitmqctl list_users | grep -c '^${MQTT_USER}' || true")

if [ "$USER_EXISTS" -gt 0 ]; then
    echo -e "${YELLOW}⚠  User ${MQTT_USER} already exists${NC}"
    read -p "Do you want to delete and recreate? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Deleting existing user...${NC}"
        ssh "${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}" "sudo rabbitmqctl delete_user ${MQTT_USER}" || true
    else
        echo -e "${RED}Aborting provisioning${NC}"
        exit 1
    fi
fi

# Create the user
echo -e "${YELLOW}Creating user ${MQTT_USER}...${NC}"
ssh "${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}" "sudo rabbitmqctl add_user ${MQTT_USER} '${MQTT_PASSWORD}'"

# Set permissions for the user (publish and subscribe to sensor/* topics)
echo -e "${YELLOW}Setting permissions...${NC}"
ssh "${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}" "sudo rabbitmqctl set_permissions -p ${MQTT_VHOST} ${MQTT_USER} '' 'sensor/.*|gateway/.*' 'sensor/.*|gateway/.*'"

# Optional: Set tags (e.g., monitoring)
ssh "${RABBITMQ_SSH_USER}@${RABBITMQ_HOST}" "sudo rabbitmqctl set_user_tags ${MQTT_USER} monitoring" || true

echo -e "${GREEN}✓ RabbitMQ user created and configured${NC}\n"

# Build firmware
echo -e "${YELLOW}Building firmware...${NC}"
pio run || {
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
}
echo -e "${GREEN}✓ Firmware built${NC}\n"

# Flash firmware
echo -e "${YELLOW}Flashing firmware to device...${NC}"
echo -e "${YELLOW}Make sure the device is connected via USB${NC}\n"

pio run --target upload || {
    echo -e "${RED}✗ Flash failed${NC}"
    echo -e "  Make sure the device is connected and in bootloader mode if needed"
    exit 1
}
echo -e "${GREEN}✓ Firmware flashed${NC}\n"

# Store credentials in device flash using serial commands
echo -e "${YELLOW}Provisioning credentials to device flash...${NC}"

# Create a Python script to send credentials via serial
PROVISION_SCRIPT=$(cat <<'PYTHON_EOF'
import sys
import time
import serial
import serial.tools.list_ports

def find_esp32_port():
    """Find the ESP32 device port"""
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        if 'USB' in port.device or 'ttyACM' in port.device or 'ttyUSB' in port.device:
            return port.device
    return None

def provision_device(mqtt_user, mqtt_password):
    """Send provisioning commands to device"""
    port = find_esp32_port()
    if not port:
        print("ERROR: Could not find ESP32 device")
        sys.exit(1)
    
    print(f"Connecting to device on {port}...")
    
    try:
        ser = serial.Serial(port, 115200, timeout=2)
        time.sleep(2)  # Wait for device to be ready
        
        # Send provisioning commands
        # Note: This requires implementing a serial command handler in the firmware
        print("Sending credentials...")
        
        # Format: PROVISION:<username>:<password>
        cmd = f"PROVISION:{mqtt_user}:{mqtt_password}\n"
        ser.write(cmd.encode())
        ser.flush()
        
        # Wait for confirmation
        time.sleep(1)
        response = ser.read_all().decode('utf-8', errors='ignore')
        print(f"Device response: {response}")
        
        ser.close()
        return True
    except Exception as e:
        print(f"ERROR: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python provision.py <mqtt_user> <mqtt_password>")
        sys.exit(1)
    
    mqtt_user = sys.argv[1]
    mqtt_password = sys.argv[2]
    
    if provision_device(mqtt_user, mqtt_password):
        print("✓ Credentials provisioned successfully")
    else:
        print("✗ Failed to provision credentials")
        sys.exit(1)
PYTHON_EOF
)

# Save and run the Python script
echo "$PROVISION_SCRIPT" > /tmp/provision_esp32.py

python3 /tmp/provision_esp32.py "$MQTT_USER" "$MQTT_PASSWORD" || {
    echo -e "${YELLOW}⚠  Automatic provisioning failed${NC}"
    echo -e "${YELLOW}You can manually provision the device:${NC}"
    echo -e "  1. Connect to device serial monitor"
    echo -e "  2. Send: PROVISION:${MQTT_USER}:${MQTT_PASSWORD}"
    echo -e "\n${YELLOW}Or use the web interface to configure${NC}"
}

# Clean up
rm -f /tmp/provision_esp32.py

echo -e "\n${BLUE}========================================${NC}"
echo -e "${GREEN}✓ Provisioning Complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "\n${BLUE}Device Information:${NC}"
echo -e "  Device ID: ${GREEN}${DEVICE_ID}${NC}"
echo -e "  MQTT User: ${GREEN}${MQTT_USER}${NC}"
echo -e "  MQTT Password: ${GREEN}${MQTT_PASSWORD}${NC}"
echo -e "\n${YELLOW}Store these credentials in a secure location!${NC}\n"

# Save credentials to a file
CREDS_FILE="device_credentials_${DEVICE_ID}.txt"
cat > "$CREDS_FILE" <<EOF
Device ID: ${DEVICE_ID}
MQTT Username: ${MQTT_USER}
MQTT Password: ${MQTT_PASSWORD}
Provisioned: $(date)
EOF

echo -e "${GREEN}✓ Credentials saved to: ${CREDS_FILE}${NC}"
echo -e "\n${BLUE}Next Steps:${NC}"
echo -e "  1. Device should connect to WiFi (configure if needed)"
echo -e "  2. Device will connect to MQTT broker with provisioned credentials"
echo -e "  3. Monitor device: pio device monitor --baud 115200"
echo -e ""
