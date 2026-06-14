# BreathHome — Protocol Specification

## Sub-GHz Mesh Protocol

### Physical Layer

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915.0 MHz (US) |
| Modulation | LoRa |
| Spreading Factor | SF7 (normal) / SF9 (long range alerts) |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| TX Power | +14 dBm (EU) / +20 dBm (US) |
| Preamble | 8 symbols |
| Sync Word | 0xBHEA (BreathHome) |
| Range | 30m indoor (SF7) / 150m (SF9) |
| Data Rate | ~5 kbps (SF7) / ~1 kbps (SF9) |

### MAC Layer — TDMA

The hub is the mesh coordinator. It assigns time slots to all nodes and broadcasts sync packets.

```
Frame: 900ms (18 slots × 50ms)

Slot 0:   Hub broadcast (sync + commands + AQI summary)
Slot 1-8: Room sensors 1-8 uplink air quality data
Slot 9:   HVAC controller uplink status
Slot 10-15: Room sensors 9-14 (if present)
Slot 16:  Reserved / expansion
Slot 17:  Alert / retransmit / emergency

Alert Override: Fall/critical gas detection triggers immediate
transmission on slot 17 regardless of TDMA schedule (CSMA fallback).
```

### Packet Format

```
Byte  0-1:  Preamble (0xAA, 0x55)
Byte  2:    Length (total packet length)
Byte  3:    Source Node ID (0=Hub, 1-16=Sensors, 17=HVAC)
Byte  4:    Destination Node ID (0xFF=Broadcast)
Byte  5:    Message Type
Byte  6-7:  Sequence Number (16-bit, big-endian)
Byte  8-55: Payload (0-48 bytes, type-dependent)
Byte 56-57: CRC16-CCITT (over bytes 3-55)

Total: 58 bytes max
```

### Message Types

| Type | Code | Direction | Payload Size | Description |
|------|------|-----------|-------------|-------------|
| AIR_QUALITY | 0x01 | Sensor→Hub | 47 | PM2.5, PM10, CO2, VOC, HCHO, temp, RH, pressure, AQI, mold, light, radon |
| RADON_DATA | 0x02 | Sensor→Hub | 8 | Radon Bq/m3, 1h avg, 24h avg |
| MOLD_RISK | 0x03 | Sensor→Hub | 4 | Mold growth risk % |
| HVAC_COMMAND | 0x04 | Hub→HVAC | 3 | Command, room, value |
| HVAC_STATUS | 0x05 | HVAC→Hub | 20 | Vent positions, purifier, filter, pressure, temp, current |
| FILTER_ALERT | 0x06 | HVAC→Hub | 4 | Filter remaining %, recommended date |
| ACK | 0x07 | Any→Any | 2 | Acknowledgment |
| OTA_BLOCK | 0x08 | Hub→Node | 48 | Firmware update chunk |
| DANGER_ALERT | 0x09 | Any→Hub | 6 | Alert type, value, AQI category |
| CALIBRATION | 0x0A | Hub→Sensor | 16 | Sensor calibration data |
| HEARTBEAT | 0x0B | Node→Hub | 8 | Alive signal, battery, uptime |
| EXPOSURE_DATA | 0x0C | Hub→Cloud | 20 | Wearable tag exposure relay |

### AIR_QUALITY Payload (0x01)

```
Offset  Size  Field
0       4     PM2.5 (float, μg/m³)
4       4     PM10 (float, μg/m³)
8       4     CO2 (float, ppm)
12      4     VOC Index (float, 0-500)
16      4     HCHO (float, ppm)
20      4     Temperature (float, °C)
24      4     Humidity (float, %)
28      4     Pressure (float, hPa)
32      2     AQI Score (uint16, 0-500)
34      1     AQI Category (uint8, 0-5)
35      4     Mold Risk % (float, 0-100)
39      2     Light Level (uint16, lux)
41      4     Radon (float, Bq/m³, 0 if no sensor)
45      2     (reserved)
Total: 47 bytes
```

### HVAC_COMMAND Payload (0x04)

```
Offset  Size  Field
0       1     Command (0=vent_pos, 1=purifier_speed, 2-7=relay on/off)
1       1     Room ID (0-7 for vents, 0xFF for all)
2       1     Value (0-100 for vent position, 0-4 for purifier speed, 0/1 for relay)
```

### DANGER_ALERT Payload (0x09)

```
Offset  Size  Field
0       1     Alert Type (0=PM25, 1=CO2, 2=VOC, 3=HCHO, 4=RADON, 5=MOLD)
1       4     Value (float, the triggering reading)
5       1     AQI Category (uint8)
```

## BLE Protocol (Wearable Tag ↔ Hub)

### GATT Service: BreathHome Tag

**Service UUID:** 0xBREA

| Characteristic | UUID | Properties | Size | Description |
|---------------|------|------------|------|-------------|
| Air Quality | 0xBH01 | Read, Notify | 4 bytes | AQI, PM2.5, CO2/10, VOC |
| Symptom Log | 0xBH02 | Write | 1 byte | 0=none, 1=wheeze, 2=cough, 3=SOBOE, 4=throat |
| Activity | 0xBH03 | Read, Notify | 1 byte | 0=still, 1=walking, 2=running, 3=sleeping |
| Battery | 0xBH04 | Read, Notify | 1 byte | Battery percentage 0-100 |
| Vibrate Alert | 0xBH05 | Write | 1 byte | 0=off, 1=short, 2=long, 3=pattern |
| Tag Config | 0xBH06 | Write | 4 bytes | Alert thresholds, LED mode, vibrate mode |

### Advertising Packet

```
Type: Non-connectable (connectionless mode)
Interval: 1000ms (1 second)
Data:
  [ Flags(3) | Complete Local Name("BH-TAG-XXXX") | Manufacturer Data:
    [ CompanyID(0xBREA) | TagID(2) | BatteryPct(1) | AQI(1) | Activity(1) | SymptomFlag(1) ] ]
```

## Zigbee Protocol (HVAC Controller ↔ Smart Devices)

### Custom Cluster: BreathHome (0xBREA)

| Attribute | ID | Type | Access | Description |
|-----------|-----|------|--------|-------------|
| Vent Position | 0x0001 | uint8 | Read/Write | 0-100% open |
| Purifier Speed | 0x0002 | uint8 | Read/Write | 0=off, 1=low, 2=med, 3=high, 4=auto |
| Thermostat Override | 0x0003 | int16 | Read/Write | Target temp × 100 (e.g., 2200 = 22.0°C) |
| Filter Health | 0x0004 | uint8 | Read Only | 0-100% remaining |
| Humidity Target | 0x0005 | uint8 | Read/Write | Target RH % (0=disabled) |

### Supported Device Types

| Type | Examples | Control |
|------|----------|---------|
| Smart Vent | Keen Home, EcoNet, Flair | Vent position 0-100% |
| Air Purifier | Coway, Levoit (Zigbee), Blueair | Speed 0-4 |
| Smart Thermostat | ecobee, Honeywell | Setpoint override |
| Humidifier/Dehumidifier | Various Zigbee | Target RH |
| Range Hood | Custom Zigbee relay | On/off |

## 433MHz Protocol (Dumb Appliances)

The HVAC controller uses an FS1000A 433MHz ASK transmitter to control non-smart appliances via learned RF codes.

| Appliance | Code | Protocol |
|-----------|------|----------|
| Range Hood On | 0x55AA55AA | Custom OOK |
| Range Hood Off | 0xAA55AA55 | Custom OOK |
| Bathroom Exhaust On | 0x33CC33CC | Custom OOK |
| Bathroom Exhaust Off | 0xCC33CC33 | Custom OOK |
| Whole House Fan On | 0x0F0F0F0F | Custom OOK |
| Whole House Fan Off | 0xF0F0F0F0 | Custom OOK |

Encoding: 12ms preamble, then Manchester-encoded 32-bit code, 3 repeats per command.

## CRC16-CCITT

Used for all mesh packets. Polynomial: 0x1021, Init: 0xFFFF.

```c
uint16_t crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}
```