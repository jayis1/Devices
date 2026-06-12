# Aqua Guard — Architecture & Protocol Docs

## Mesh Protocol Specification

### Physical Layer
- **Radio:** SX1261/62, 868MHz (EU) / 915MHz (US)
- **Modulation:** LoRa — SF7 (normal mode), SF10 (long range / emergency)
- **Bandwidth:** 125kHz
- **TX Power:** +14dBm (EU limit), +20dBm (US)
- **Range:** 30m indoor (typical), 200m (long range mode)

### MAC Layer
- **Access:** TDMA (Time Division Multiple Access)
- **Hub is coordinator:** assigns slots, manages sync
- **Frame:** 10 slots × 100ms = 1 second
- **Slot 0:** Hub broadcast (sync + commands)
- **Slots 1-7:** Sensor node uplinks
- **Slot 8:** Feeder node uplink
- **Slot 9:** Control / ACK / retransmit

### Network Layer
- **Addressing:** 8-bit node IDs (0x00=hub, 0x01-0x07=sensors, 0x08=feeder)
- **Broadcast:** 0xFF destination address
- **Join procedure:** New node transmits in slot 9 with NODE_ID=0xFF, hub assigns ID
- **Timeout:** Node marked inactive after 30s without heartbeat

### Application Layer
- Packet types: SENSOR_DATA, FEEDER_STATUS, COMMAND, ACK, OTA_BLOCK, CALIBRATION, ALARM, HEARTBEAT
- See `firmware/common/mesh_protocol.h` for full struct definitions

## WiFi / BLE Bridge (ESP32-C6 on Hub)

### MQTT Topics
- `aquaguard/sensor_data/{node_id}` — Sensor readings (JSON)
- `aquaguard/feeder_status` — Feeder node status
- `aquaguard/alerts` — System alerts
- `aquaguard/commands/dose` — Dosing commands
- `aquaguard/commands/feed` — Feeding commands
- `aquaguard/commands/light` — Lighting overrides
- `aquaguard/ota/{node_id}` — OTA firmware blocks

### BLE GATT Service
- Service UUID: `0xFFB0` (AquaGuard)
- Char `0xFFB1`: All sensor data (read/notify)
- Char `0xFFB2`: Feeder status (read)
- Char `0xFFB3`: Command write (write)
- Char `0xFFB4`: System status (read/notify)
- Char `0xFFB5`: WiFi config (write)
- Char `0xFFB6`: Device info (read)

## Alert Priority Levels

| Level | Notification | Local Action |
|-------|-------------|-------------|
| INFO | Dashboard only | LED blue blink |
| WARNING | Push notification | LED orange + 2 beeps |
| CRITICAL | Push + SMS | LED red + alarm buzzer + auto-dose |
| EMERGENCY | Push + SMS + email | Continuous alarm + auto-shutdown + emergency dose |

## Dosing Engine Logic

```
1. Read current parameters from sensor node
2. Compare against species-safe ranges
3. If parameter below/above range:
   a. Look up dosing table (chemical → amount per liter)
   b. Calculate tank volume adjustment
   c. Apply ML correction factor (based on historical response)
   d. Send CMD_DOSE to feeder node
   e. Log dose to cloud + local
4. Monitor response over next 30 minutes
5. If no improvement, escalate to next dose level
6. If worsening, trigger CRITICAL alert
```

## OTA Update Protocol

1. Dashboard uploads new firmware binary to cloud
2. Cloud sends OTA_BLOCK packets via MQTT → hub
3. Hub caches blocks, verifies CRC per block
4. Hub broadcasts OTA_BLOCK to target node in slot 9
5. Target node writes to flash, verifies, sends ACK
6. On all blocks received + verified, target reboots into new firmware
7. Hub monitors heartbeat from updated node
8. If no heartbeat in 60s, rollback to previous firmware