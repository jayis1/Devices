# FireSync — Protocol Specification

## Physical & Link Layer

- **Band:** 868 MHz Sub-GHz (SX1262 LoRa radio, all nodes)
- **Modulation:** LoRa, SF7, BW 125 kHz, +22 dBm
- **Topology:** TDMA mesh — Hub as coordinator, all nodes as relays
- **TDMA:** 17 slots × 250 ms = 4.25 s cycle; slot 0=Hub, 1-14=nodes, 15=priority, 16=reserved
- **Emergency preemption:** FIRE_ALERT uses priority slot (15), transmitted 3× immediately (<2s latency)
- **Encryption:** AES-128-CTR (application layer, per-node key)
- **CRC:** CRC-16-CCITT (application layer end-to-end integrity)
- **ACK:** FIRE_ALERT and suppression commands require ACK (3 retries, 500 ms timeout)
- **Range:** 100 m indoor (penetrates 3+ walls), 2 km LOS
- **Max nodes:** 20 (16 sentinels + stove + panel + escape + hub)

## Message Format

All Sub-GHz packets use a compact binary protocol with application-layer CRC:

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16(2) │
│ 0x46 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

- **Sync:** `0x46 0x53` = "FS" (FireSync)
- **Src:** Source node ID (0=Hub, 1-14=nodes, 0x10=Stove, 0x11=Panel, 0x12=Escape)
- **Dst:** Destination (0=Hub, 0xFF=Broadcast)
- **MsgType:** Message type (see below)
- **MsgId:** 16-bit sequence number
- **Payload:** Variable length (0-240 bytes)
- **CRC:** CRC-16-CCITT over bytes [2..len-3] (Src through Payload end)

Sub-GHz is a shared medium (no link-layer encryption/CRC like BLE), so
application-layer CRC + AES encryption are essential.

## Message Types

| Type | Name | Direction | Priority | Payload |
|------|------|-----------|----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Normal | 4 bytes |
| 0x02 | JOIN_ACK | Hub→Node | Normal | 1 byte |
| 0x03 | TELEMETRY | Node→Hub | Normal | 12-24 bytes |
| 0x04 | COMMAND | Hub→Node | Normal | 1+N bytes |
| 0x05 | CMD_ACK | Node→Hub | Normal | 2 bytes |
| 0x06 | FIRE_ALERT | Node→Hub | **EMERGENCY** | 12 bytes |
| 0x07 | FIRE_CONFIRM | Hub→All | **EMERGENCY** | variable |
| 0x08 | ALARM_TRIGGER | Hub→All | **EMERGENCY** | 0 bytes |
| 0x09 | ALARM_STOP | Hub→All | High | 0 bytes |
| 0x0A | ESCAPE_UPDATE | Hub→Escape | **EMERGENCY** | 8 bytes |
| 0x0B | STOVE_SHUTOFF | Hub→Stove | High | 0 bytes |
| 0x0C | PANEL_SHUTOFF | Hub→Panel | High | 0 bytes |
| 0x0D | HVAC_SHUTOFF | Hub→All | High | 0 bytes |
| 0x0E | DOOR_RELEASE | Hub→Escape | **EMERGENCY** | 1 byte |
| 0x0F | OTA_BLOCK | Hub→Node | Low | variable |
| 0x10 | OTA_ACK | Node→Hub | Low | 4 bytes |
| 0x11 | HEARTBEAT | Node→Hub | Normal | 4 bytes |
| 0x12 | OCCUPANT | Sentinel→Hub | Normal | 2 bytes |
| 0x13 | FIRE_DISPATCH | Hub→Cloud | **EMERGENCY** | variable |
| 0x14 | SUPPRESS_STATUS | Node→Hub | High | variable |
| 0x15 | CALIBRATION | Hub→Node | Normal | variable |
| 0x16 | CALIB_ACK | Node→Hub | Normal | 2 bytes |
| 0x17 | TIME_SYNC | Hub→All | Normal | 4 bytes |
| 0x18 | SILENCE | Hub→Node | Normal | 0 bytes |
| 0x19 | TEST_ALARM | Hub→All | Normal | 0 bytes |

## Fire Alert Payload (12 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | fire_class | 1 | FlameNet class (5=smoldering, 6=flaming_fire) |
| 1 | confidence | 1 | FlameNet confidence (0-100%) |
| 2 | room_id | 1 | Room identifier (0-15, 0xFF=stove, 0xFE=panel) |
| 3-4 | smoke_pm25 | 2 | Smoke density at detection (μg/m³) |
| 5-6 | co_ppm | 2 | CO at detection (ppm) |
| 7-8 | temp_c_x10 | 2 | Temperature at detection (×0.1°C, signed) |
| 9-10 | thermal_max_x10 | 2 | Thermal array max (×0.1°C, signed) |
| 11 | occupant | 1 | PIR occupant present (0=no, 1=yes) |

## Escape Route Payload (8 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | fire_room | 1 | Room ID where fire detected |
| 1 | safe_exit | 1 | Recommended exit (0=front, 1=back, 2=garage, 3=window) |
| 2-3 | avoid_rooms | 2 | Bitmask of rooms to avoid (fire + adjacent) |
| 4 | led_path_mask | 1 | LED strips to activate green (bitmask) |
| 5 | led_red_mask | 1 | LED strips to activate red (bitmask) |
| 6 | voice_msg_id | 1 | Voice guidance message ID (0-11) |
| 7 | door_mask | 1 | Doors to release (bitmask) |

## Telemetry Payloads

### Sentinel Telemetry (24 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x01 (SENTINEL) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2-3 | smoke_pm25 | 2 | PM2.5 concentration (μg/m³) |
| 4-5 | co_ppm | 2 | CO concentration (ppm) |
| 6-7 | temp_c_x10 | 2 | Temperature (×0.1°C, signed) |
| 8 | temp_rate | 1 | Rate of temperature rise (°C/min, signed) |
| 9-10 | thermal_max_x10 | 2 | MLX90640 max zone temp (×0.1°C) |
| 11-12 | thermal_mean_x10 | 2 | MLX90640 mean temp (×0.1°C) |
| 13 | flame_class | 1 | FlameNet output (0-6) |
| 14 | flame_confidence | 1 | FlameNet confidence (0-100%) |
| 15 | pir_occupant | 1 | 0=empty, 1=occupied |
| 16-17 | flamenet_ms | 2 | FlameNet inference time (ms) |
| 18 | thermal_anomaly | 1 | ThermalAnomaly score (0-255) |
| 19-20 | free_heap | 2 | Free heap (bytes) |
| 21 | rssi | 1 | Sub-GHz RSSI (dBm, signed) |
| 22-23 | uptime_min | 2 | Uptime (minutes) |

### Stove Telemetry (18 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x02 (STOVE) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2-3 | thermal_max_x10 | 2 | MLX90640 max pan temp (×0.1°C) |
| 4-5 | thermal_mean_x10 | 2 | MLX90640 mean temp (×0.1°C) |
| 6 | knob_positions | 1 | Bitmask: bits 0-3 on/off, bits 4-7 level |
| 7 | pantemp_class | 1 | PanTemp CNN output (0-3) |
| 8-9 | timer_remaining_s | 2 | Auto-shutoff timer (seconds) |
| 10 | valve_state | 1 | 0=open, 1=closed |
| 11 | buzzer_active | 1 | 0=off, 1=on |
| 12-13 | free_heap | 2 | Free heap (bytes) |
| 14 | rssi | 1 | Sub-GHz RSSI (dBm, signed) |
| 15-16 | uptime_min | 2 | Uptime (minutes) |

### Panel Telemetry (16 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x03 (PANEL) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2-3 | main_current_x100 | 2 | Main current (×0.01A) |
| 4-5 | voltage_x10 | 2 | Mains voltage (×0.1V) |
| 6-7 | power_x10 | 2 | Real power (×0.1W) |
| 8 | bus_bar_temp_c | 1 | Bus bar temperature (°C, signed) |
| 9 | breaker_temp_c | 1 | Hottest breaker temperature (°C, signed) |
| 10 | arc_fault_class | 1 | ArcDetect output (0-3) |
| 11 | arc_confidence | 1 | ArcDetect confidence (0-100%) |
| 12 | shunt_tripped | 1 | 0=normal, 1=tripped |
| 13 | rssi | 1 | Sub-GHz RSSI (dBm, signed) |
| 14-15 | uptime_min | 2 | Uptime (minutes) |

### Escape Telemetry (12 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x04 (ESCAPE) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2 | led_zones | 1 | Active LED zone bitmask |
| 3 | speaker_active | 1 | 0=off, 1=on |
| 4 | doors_released | 1 | Door release bitmask |
| 5 | route_active | 1 | 0=inactive, 1=active |
| 6-7 | free_heap | 2 | Free heap (bytes) |
| 8 | rssi | 1 | Sub-GHz RSSI (dBm, signed) |
| 9-10 | uptime_min | 2 | Uptime (minutes) |

## Voice Guidance Messages

| ID | Message (English) | When |
|----|-------------------|------|
| 0 | "Fire detected in the kitchen. Exit through the front door." | Kitchen fire, front exit |
| 1 | "Fire detected in the living room. Exit through the back door." | Living room fire, back exit |
| 2 | "Fire detected. Leave the house immediately." | Generic fire |
| 3 | "Do not use the stairs. Fire in the stairwell." | Stairwell fire |
| 4 | "Fire in the bedroom. Exit through the window." | Bedroom fire, window exit |
| 5 | "Smoke detected. Move to the nearest exit." | Smoke without confirmed fire |
| 6 | "Fire has been contained. You may return." | All clear |
| 7 | "This is a test of the FireSync system." | Monthly test |
| 8 | "Gas shutoff activated at the stove." | Stove shutoff |
| 9 | "Electrical hazard detected. Power disconnected." | Panel shunt trip |
| 10 | "Carbon monoxide detected. Evacuate and ventilate." | CO alarm |
| 11 | "Battery backup active. Fire monitoring continues." | Power outage |

Available in 8 languages: EN, ES, ZH, FR, DE, JA, KO, PT

## Alert Types

| Type | Name | Severity | Description |
|------|------|----------|-------------|
| 0x01 | LOW_BATTERY | WARNING | Battery below threshold |
| 0x02 | SMOKE_WARN | WARNING | Smoke above warning threshold |
| 0x03 | SMOKE_FIRE | EMERGENCY | Fire-confirmed smoke |
| 0x04 | CO_WARN | WARNING | CO >35 ppm |
| 0x05 | CO_DANGER | CRITICAL | CO >100 ppm |
| 0x06 | CO_CRITICAL | EMERGENCY | CO >400 ppm |
| 0x07 | THERMAL_WARN | WARNING | Thermal array above warning |
| 0x08 | THERMAL_FIRE | EMERGENCY | Thermal array fire-level |
| 0x09 | ARC_FAULT | CRITICAL | Arc fault detected |
| 0x0A | OVERLOAD | WARNING | Sustained overload |
| 0x0B | STOVE_SHUTOFF | INFO | Stove gas valve closed |
| 0x0C | PANEL_TRIP | CRITICAL | Shunt-trip breaker activated |
| 0x0D | NODE_OFFLINE | WARNING | Node lost mesh connection |
| 0x0E | SENSOR_ANOMALY | WARNING | Sensor fault detected |
| 0x0F | FIRE_DISPATCH | EMERGENCY | 911 dispatched |
| 0x10 | TEST_ALARM | INFO | Monthly test |