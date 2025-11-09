# BLE Gateway Architecture - v1.1.0

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        BLE Gateway v1.1.0                       │
│                     (ESP32 Dual-Core System)                    │
└─────────────────────────────────────────────────────────────────┘
                                │
                    ┌───────────┴────────────┐
                    │                        │
            ┌───────▼───────┐        ┌──────▼──────┐
            │    Core 0     │        │   Core 1    │
            │ (Network/IO)  │        │ (BLE Radio) │
            └───────┬───────┘        └─────┬───────┘
                    │                      │
        ┌───────────┼───────────┐          │
        │           │           │          │
  ┌─────▼─────┐  ┌──▼───┐   ┌───▼────┐ ┌───▼────┐
  │   MQTT    │  │Config│   │Message │ │  BLE   │
  │Maintenance│  │Portal│   │Process │ │ Scan   │
  │  Task     │  │Server│   │ Task   │ │ Task   │
  │Priority: 2│  │(Web) │   │Prior:1 │ │Prior:1 │
  └─────┬─────┘  └──┬───┘   └───┬────┘ └───┬────┘
        │           │           │          │
        │           │           │          │
        └───────────┴───────────┴──────────┘
                    │
        ┌───────────┴────────────┐
        │   Shared Resources     │
        │   (Mutex Protected)    │
        │                        │
        │  • Detection Buffer    │
        │  • MQTT Client         │
        │  • Configuration       │
        └────────────────────────┘
```

## Task Architecture

### Core 0 - Network and Processing
```
┌────────────────────────────────────────────────┐
│          MQTT Maintenance Task                 │
│              (Priority 2)                      │
├────────────────────────────────────────────────┤
│  • Monitors WiFi connection                    │
│  • Maintains MQTT keepalive                    │
│  • Processes incoming messages                 │
│  • Handles RPC commands                        │
│  • Auto-reconnect every 30s if down            │
│  • Sends gateway status (every 5 min)          │
│  • Manages OTA updates                         │
│                                                │
│  Loop: Every 100ms                             │
└────────────────────────────────────────────────┘

┌────────────────────────────────────────────────┐
│         Message Processing Task                │
│              (Priority 1)                      │
├────────────────────────────────────────────────┤
│  • Batches BLE device data                     │
│  • Sends telemetry to ThingsBoard              │
│  • Thread-safe buffer access                   │
│  • Publishes device attributes                 │
│                                                │
│  Loop: Every 60 seconds                        │
└────────────────────────────────────────────────┘

┌────────────────────────────────────────────────┐
│          Web Configuration Server              │
│           (Main Loop, Config Mode)             │
├────────────────────────────────────────────────┤
│  • Serves web interface on AP mode             │
│  • Handles DNS requests                        │
│  • Processes configuration saves               │
│  • Encrypted credential storage                │
│                                                │
│  Active: When in config mode only              │
└────────────────────────────────────────────────┘
```

### Core 1 - Bluetooth Operations
```
┌────────────────────────────────────────────────┐
│            BLE Scanning Task                   │
│              (Priority 1)                      │
├────────────────────────────────────────────────┤
│  • Continuous BLE scanning                     │
│  • Detects BLE advertisements                  │
│  • Parses sensor data                          │
│  • Updates detection buffer (thread-safe)      │
│  • Deduplication logic                         │
│                                                │
│  Loop: Every 10 seconds (5s scan window)       │
└────────────────────────────────────────────────┘
```

## Startup Sequence

```
┌─────────────┐
│   Power On  │
└──────┬──────┘
       │
┌──────▼─────────────────────────────────┐
│  1. Initialize Hardware                │
│     • Serial console (115200 baud)     │
│     • EEPROM                           │
│     • BLE radio                        │
└──────┬─────────────────────────────────┘
       │
┌──────▼─────────────────────────────────┐
│  2. Load Configuration                 │
│     • Read from EEPROM                 │
│     • Decrypt credentials              │
│     • Validate config                  │
└──────┬─────────────────────────────────┘
       │
       ├── Config Invalid ──┐
       │                    │
       │              ┌─────▼──────┐
       │              │  AP Mode   │
       │              │ (Config    │
       │              │  Portal)   │
       │              └────────────┘
       │
┌──────▼─────────────────────────────────┐
│  3. Connect to WiFi                    │
│     • Try saved credentials            │
│     • Wait up to 10 seconds            │
└──────┬─────────────────────────────────┘
       │
       ├── WiFi Failed ───┐
       │                  │
       │            ┌─────▼──────┐
       │            │  AP Mode   │
       │            └────────────┘
       │
┌──────▼─────────────────────────────────┐
│  4. Sync Time (NTP)                    │
│     • Connect to NTP servers           │
│     • Set system time                  │
│     • Continue if fails                │
└──────┬─────────────────────────────────┘
       │
┌──────▼─────────────────────────────────┐
│  5. Connect to MQTT                    │
│     • Try saved credentials (3x)       │
└──────┬─────────────────────────────────┘
       │
       ├── MQTT Failed ──┐
       │                 │
       │         ┌───────▼────────┐
       │         │ Config URL     │
       │         │ Fallback       │
       │         │ • Fetch new    │
       │         │   credentials  │
       │         │ • Retry MQTT   │
       │         └────────────────┘
       │
┌──────▼─────────────────────────────────┐
│  6. Create FreeRTOS Tasks              │
│     • MQTT Maintenance (Core 0)        │
│     • BLE Scanning (Core 1)            │
│     • Message Processing (Core 0)      │
└──────┬─────────────────────────────────┘
       │
┌──────▼─────────────────────────────────┐
│  7. Normal Operation                   │
│     • All tasks running                │
│     • Main loop monitors health        │
└────────────────────────────────────────┘
```

## Data Flow

### BLE Device Discovery and Publishing

```
┌──────────────┐
│ BLE Devices  │
│  Advertising │
└──────┬───────┘
       │ (BLE Radio)
       │
┌──────▼─────────────────────────────┐
│  BLE Scan Task (Core 1)            │
│  • Receive advertisement           │
│  • Parse service data              │
│  • Extract sensor values           │
│  • Calculate data hash             │
└──────┬─────────────────────────────┘
       │
       │ (Take Mutex)
       │
┌──────▼─────────────────────────────┐
│  Detection Buffer                  │
│  • Check for duplicates            │
│  • Update or add entry             │
│  • Store: MAC, RSSI, data, hash    │
└──────┬─────────────────────────────┘
       │ (Release Mutex)
       │
       │ (Wait 60 seconds)
       │
┌──────▼─────────────────────────────┐
│  Message Processing Task (Core 0)  │
│  • Take Mutex                      │
│  • Read all detections             │
│  • Build JSON payloads             │
│  • Release Mutex                   │
└──────┬─────────────────────────────┘
       │
       │ (Take MQTT Mutex)
       │
┌──────▼─────────────────────────────┐
│  MQTT Publishing                   │
│  1. v1/gateway/connect             │
│     • Device list with types       │
│  2. v1/gateway/attributes          │
│     • Device metadata              │
│  3. v1/gateway/telemetry           │
│     • Sensor readings, RSSI        │
└──────┬─────────────────────────────┘
       │ (Release MQTT Mutex)
       │
┌──────▼─────────────────────────────┐
│  ThingsBoard Platform              │
│  • Store telemetry                 │
│  • Update device status            │
│  • Trigger rules/alarms            │
└────────────────────────────────────┘
```

### MQTT Connection Management

```
┌────────────────────────────────────┐
│  MQTT Maintenance Task             │
│  (Runs every 100ms)                │
└──────┬─────────────────────────────┘
       │
       ├─ Connected? ─ Yes ──┐
       │                     │
       │                ┌────▼────┐
       │                │  Loop   │
       │                │ Process │
       │                │Messages │
       │                └─────────┘
       │
       No
       │
┌──────▼─────────────────────────────┐
│  Connection Lost                   │
│  • Log disconnect time             │
│  • Set mqtt_connected = false      │
└──────┬─────────────────────────────┘
       │
       │ (Wait 30 seconds)
       │
┌──────▼─────────────────────────────┐
│  Reconnection Attempt              │
│  • Take MQTT Mutex                 │
│  • Try to connect (3 attempts)     │
│  • Release Mutex                   │
└──────┬─────────────────────────────┘
       │
       ├─ Success? ─ Yes ──┐
       │                   │
       │              ┌────▼────────┐
       │              │ Subscribe   │
       │              │ to topics   │
       │              │ Send status │
       │              └─────────────┘
       │
       No (after 5 min)
       │
┌──────▼─────────────────────────────┐
│  Config URL Fallback               │
│  • Fetch updated credentials       │
│  • Save to EEPROM                  │
│  • Retry connection                │
└──────┬─────────────────────────────┘
       │
       ├─ Success? ─ No ──────┐
       │                      │
       │              ┌───────▼──────┐
       │              │ Enter AP Mode│
       │              │ (User Config)│
       │              └──────────────┘
       │
       Yes
       │
┌──────▼─────────────────────────────┐
│  Resume Normal Operation           │
└────────────────────────────────────┘
```

## Thread Synchronization

### Mutex Protection

```
┌─────────────────────────────────────────┐
│      detectionBufferMutex               │
├─────────────────────────────────────────┤
│  Protects: Detection Buffer (std::map)  │
│                                         │
│  Used by:                               │
│  • BLE Scan Task (write)                │
│  • Message Processing Task (read/clear) │
│                                         │
│  Critical Sections:                     │
│  • Adding new device                    │
│  • Checking for duplicates              │
│  • Updating RSSI                        │
│  • Batch reading all devices            │
│  • Clearing buffer after send           │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│           mqttMutex                     │
├─────────────────────────────────────────┤
│  Protects: MQTT Client Operations       │
│                                         │
│  Used by:                               │
│  • MQTT Maintenance Task                │
│  • Message Processing Task              │
│                                         │
│  Critical Sections:                     │
│  • Connecting/disconnecting             │
│  • Publishing messages                  │
│  • Subscribing to topics                │
│  • Processing incoming messages         │
│  • OTA updates                          │
└─────────────────────────────────────────┘
```

## Memory Management

```
┌─────────────────────────────────────────┐
│        ESP32 Memory Layout              │
├─────────────────────────────────────────┤
│                                         │
│  Heap Memory:                           │
│  • Free at startup: ~200KB              │
│  • Task stacks: 24KB (3 x 8KB)          │
│  • MQTT buffer: 8KB                     │
│  • BLE structures: ~10KB                │
│  • Detection buffer: varies             │
│  • Typical free running: 100-150KB      │
│                                         │
│  Flash Memory:                          │
│  • Program: ~900KB                      │
│  • OTA partition: ~1.4MB                │
│  • SPIFFS: ~1.5MB (unused)              │
│  • EEPROM: 1KB (config storage)         │
│                                         │
└─────────────────────────────────────────┘
```

## Configuration Storage

```
┌─────────────────────────────────────────┐
│        EEPROM Layout (1KB)              │
├─────────────────────────────────────────┤
│  Addr  │ Size │ Content                 │
├────────┼──────┼─────────────────────────┤
│  0     │ 64B  │ WiFi SSID               │
│  64    │ 64B  │ WiFi Password (enc)     │
│  128   │ 64B  │ MQTT Host               │
│  192   │ 64B  │ MQTT Token (enc)        │
│  256   │ 1B   │ Config Valid (0xAA)     │
│  320   │ 128B │ Config URL              │
│  448   │ 64B  │ Config Username         │
│  512   │ 64B  │ Config Password (enc)   │
└─────────────────────────────────────────┘
```

## Network Communication

```
┌─────────────────────────────────────────┐
│     External Communication              │
├─────────────────────────────────────────┤
│                                         │
│  WiFi Connection:                       │
│  • 2.4GHz 802.11 b/g/n                  │
│  • WPA/WPA2 security                    │
│  • DHCP client                          │
│                                         │
│  NTP Time Sync:                         │
│  • UDP port 123                         │
│  • pool.ntp.org                         │
│  • time.nist.gov (backup)               │
│                                         │
│  MQTT Connection:                       │
│  • TCP port 1883                        │
│  • Client ID: BLEGateway-[MAC]          │
│  • QoS 0 (default)                      │
│  • Keepalive: 60 seconds                │
│                                         │
│  Config Fallback:                       │
│  • HTTPS (recommended)                  │
│  • Basic authentication (optional)      │
│  • Custom headers (MAC, version)        │
│                                         │
│  OTA Updates:                           │
│  • HTTPS download                       │
│  • Streaming write to flash             │
│  • Verification before boot             │
│                                         │
└─────────────────────────────────────────┘
```

## Error Handling and Recovery

```
┌─────────────────────────────────────────┐
│        Failure Scenarios                │
├─────────────────────────────────────────┤
│                                         │
│  WiFi Connection Failed:                │
│  → Enter AP mode immediately            │
│  → Serve configuration portal           │
│                                         │
│  MQTT Connection Failed:                │
│  → Retry every 30 seconds (3x)          │
│  → Try config URL fallback              │
│  → Retry with new credentials           │
│  → Enter AP mode after 5 minutes        │
│                                         │
│  NTP Sync Failed:                       │
│  → Log warning                          │
│  → Continue operation                   │
│  → Set timeSynced = false               │
│                                         │
│  Config URL Failed:                     │
│  → Log error                            │
│  → Continue with existing config        │
│  → Retry on next MQTT failure           │
│                                         │
│  Task Watchdog:                         │
│  → System reset                         │
│  → Auto-recovery on boot                │
│                                         │
│  Memory Low (<50KB):                    │
│  → Clear old detections                 │
│  → Reduce batch size                    │
│  → Log warning                          │
│                                         │
└─────────────────────────────────────────┘
```

## Performance Characteristics

```
┌─────────────────────────────────────────┐
│      Typical Performance Metrics        │
├─────────────────────────────────────────┤
│                                         │
│  BLE Scanning:                          │
│  • Scan every: 10 seconds               │
│  • Scan duration: 5 seconds             │
│  • Max devices per scan: 10-20          │
│  • Detection latency: <5 seconds        │
│                                         │
│  MQTT Publishing:                       │
│  • Batch interval: 60 seconds           │
│  • Max payload size: 8KB                │
│  • Publish time: 100-500ms              │
│  • Keepalive: 60 seconds                │
│                                         │
│  CPU Usage:                             │
│  • Core 0: 20-40% (MQTT + Processing)   │
│  • Core 1: 10-30% (BLE scanning)        │
│  • Idle time: 40-60% (both cores)       │
│                                         │
│  Memory Usage:                          │
│  • Free heap: 100-150KB                 │
│  • Stack usage: ~50% per task           │
│  • Buffer usage: varies with devices    │
│                                         │
│  Network:                               │
│  • Data rate: 1-5 KB/minute             │
│  • WiFi power: ~100mA active            │
│  • BLE power: ~50mA scanning            │
│                                         │
└─────────────────────────────────────────┘
```

## Scalability Limits

```
┌─────────────────────────────────────────┐
│        System Capacity                  │
├─────────────────────────────────────────┤
│                                         │
│  BLE Devices:                           │
│  • Recommended: 10-15 devices           │
│  • Maximum: 20-30 devices               │
│  • Limiting factor: Memory buffer       │
│                                         │
│  MQTT Payload:                          │
│  • Single message: 8KB max              │
│  • Batch size: 10-15 devices            │
│  • Multiple batches if needed           │
│                                         │
│  Update Rate:                           │
│  • BLE scan: 10 seconds (fixed)         │
│  • MQTT publish: 60 seconds (config)    │
│  • Can be adjusted in code              │
│                                         │
└─────────────────────────────────────────┘
```

---

**Document Version:** 1.0  
**Last Updated:** November, 2025  
**Firmware Version:** 1.1.0  
