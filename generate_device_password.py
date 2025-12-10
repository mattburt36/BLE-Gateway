#!/usr/bin/env python3
"""
Generate a unique password for a BLE Gateway device based on its MAC address.
Uses HMAC-SHA256 with a secret salt to create a deterministic but secure password.
"""

import hashlib
import hmac
import base64
import sys

# Secret salt - this should be kept secure and never shared
# Change this to your own unique secret
SECRET_SALT = "HoptechBLEGateway2025_SecretSalt_DoNotShare_KeepThisSecure"

def generate_device_password(device_id: str) -> str:
    """
    Generate a unique password for a device based on its ID.
    
    Args:
        device_id: The device ID (typically derived from MAC address)
    
    Returns:
        A base64-encoded password string (32 characters)
    """
    # Normalize device ID to uppercase
    device_id = device_id.upper().strip()
    
    # Create HMAC-SHA256 hash using secret salt and device ID
    h = hmac.new(
        SECRET_SALT.encode('utf-8'),
        device_id.encode('utf-8'),
        hashlib.sha256
    )
    
    # Get the digest and encode as base64
    # Take first 24 bytes of the hash and encode to get ~32 char password
    password_bytes = h.digest()[:24]
    password = base64.b64encode(password_bytes).decode('utf-8')
    
    return password

def generate_username(device_id: str) -> str:
    """
    Generate a username for the device.
    Format: ble-gateway-{device_id}
    """
    return f"ble-gateway-{device_id.upper()}"

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 generate_device_password.py <device_id>")
        print("Example: python3 generate_device_password.py E86909E9AB4")
        sys.exit(1)
    
    device_id = sys.argv[1]
    username = generate_username(device_id)
    password = generate_device_password(device_id)
    
    print(f"\n{'='*60}")
    print(f"BLE Gateway Device Credentials")
    print(f"{'='*60}")
    print(f"Device ID:  {device_id.upper()}")
    print(f"Username:   {username}")
    print(f"Password:   {password}")
    print(f"{'='*60}")
    print(f"\nProvisioning Command:")
    print(f"PROVISION:{username}:{password}:")
    print(f"{'='*60}\n")
