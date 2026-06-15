# MedSync - BLE Mesh Protocol Specification

## Overview

MedSync uses BLE 5.0 Mesh for all inter-node communication. The hub acts as the provisioner and coordinates the mesh network.

## Network Parameters

| Parameter | Value |
|-----------|-------|
| Protocol | BLE 5.0 Mesh |
| Frequency | 2.4 GHz ISM band |
| Channels | 37 data channels (frequency hopping) |
| Modulation | GFSK 1 Mbps / 2 Mbps |
| TX Power | Hub: +8 dBm, Beacons: +4 dBm, Wearable: +4 dBm |
| Range | 15m indoor (per hop), multi-hop extends to whole home |
| Topology | Mesh (hub = provisioner) |
| Security | AES-128-CCM, network key + application key per device type |
| Provisioning | NFC tap or numeric comparison via mobile app |

## Custom Vendor Models

### Company ID: 0x0059 (Nordic Semiconductor)

---

### Model 0xFC00: MedSync Schedule

Handles medication schedule distribution and dose events.

| Opcode | Name | Payload | Direction |
|--------|------|---------|-----------|
| 0x00 | ScheduleSet | schedule_entry[variable] | Hub → Pill Station |
| 0x01 | ScheduleGet | empty | Any → Hub |
| 0x02 | ScheduleStatus | count[1] + entries[variable] | Hub → Any |
| 0x03 | DoseTrigger | bin_id[1] + dose_count[1] + urgency[1] | Hub → Pill Station |
| 0x04 | DoseConfirm | bin_id[1] + method[1] + timestamp[4] | Pill Station/Wearable → Hub |
| 0x05 | DoseMissed | bin_id[1] + timestamp[4] | Hub → Cloud |

### Schedule Entry Format

| Field | Size | Description |
|-------|------|-------------|
| bin_id | 1 byte | Compartment index (0-7) |
| medication_id | 1 byte | Medication database ID |
| dose_count | 1 byte | Number of pills per dose |
| hour | 1 byte | Hour (0-23) |
| minute | 1 byte | Minute (0-59) |
| frequency | 1 byte | Bitmask: daily/weekly/as-needed |
| food_instruction | 1 byte | 0=anytime, 1=before, 2=with, 3=after food |
| pill_weight_mg | 2 bytes | Weight per pill (little-endian) |
| schedule_id | 4 bytes | Unique schedule entry ID |

---

### Model 0xFC01: MedSync Vitals

Health vitals from the wearable tag.

| Attribute | ID | Type | Unit |
|-----------|------|------|------|
| HeartRate | 0x0000 | uint8 | BPM |
| SpO2 | 0x0001 | uint8 | % |
| ActivityLevel | 0x0002 | enum8 | still/walking/running/sleeping/unknown |
| FallDetected | 0x0003 | boolean | true/false |
| StepsCount | 0x0004 | uint16 | steps |
| SkinTemp | 0x0005 | int16 | °C × 100 |

### Vitals Report Format

| Field | Size | Description |
|-------|------|-------------|
| node_id | 2 bytes | BLE mesh node ID |
| heart_rate_bpm | 1 byte | Heart rate |
| spo2_percent | 1 byte | SpO2 percentage |
| activity_level | 1 byte | Activity classification |
| fall_detected | 1 byte | 0=false, 1=true |
| steps_count | 2 bytes | Step counter |
| skin_temp_cx100 | 2 bytes | Skin temperature |
| battery_mv | 2 bytes | Battery voltage |
| timestamp | 4 bytes | Unix timestamp |

---

### Model 0xFC02: MedSync Pill Station

Pill station control and status.

| Attribute | ID | Type | Description |
|-----------|------|------|-------------|
| CarouselPosition | 0x0000 | uint8 | Current bin position (0-7) |
| BinWeight | 0x0001 | int32 | Per-bin weight in mg × 100 |
| BinStatus | 0x0002 | enum8 | Per-bin status |
| CoverState | 0x0003 | boolean | Cover open/closed |
| MotorFault | 0x0004 | bitmap8 | Per-motor fault flags |

| Opcode | Name | Payload | Direction |
|--------|------|---------|-----------|
| 0x00 | DispenseDose | bin_id[1] + pill_count[1] | Hub → Pill Station |
| 0x01 | RefillBin | bin_id[1] + pill_name[16] + weight[2] + count[1] | Hub → Pill Station |
| 0x02 | CalibrateScale | bin_id[1] + known_weight[4] | Hub → Pill Station |
| 0x03 | HomeCarousel | empty | Hub → Pill Station |
| 0x04 | EmergencyStop | empty | Any → Pill Station |

---

### Model 0xFC03: MedSync Alert

Alert distribution across all nodes.

| Attribute | ID | Type | Description |
|-----------|------|------|-------------|
| AlertLevel | 0x0000 | enum8 | info/reminder/warning/urgent/emergency |
| AlertType | 0x0001 | enum8 | Type of alert |
| AlertMessage | 0x0002 | string[64] | Human-readable message |

### Alert Levels

| Level | Value | Hub Action | Room Beacon | Wearable | Push Notification |
|-------|-------|-------------|-------------|----------|-------------------|
| INFO | 0 | Display update | LED pulse (blue) | LED pulse (blue) | None |
| REMINDER | 1 | Voice + display | LED + 1 beep | 2 vibes | Yes |
| WARNING | 2 | Voice + display + alarm | LED pulse + 2 beeps | 3 vibes | Yes + SMS |
| URGENT | 3 | Continuous alarm | Rapid LED + 3 beeps | Long vibe | Yes + SMS + call |
| EMERGENCY | 4 | Continuous alarm + 911 | Flashing red + alarm | Continuous vibe | Yes + SMS + call |

---

## Standard BLE Mesh Models Used

- Configuration Server/Client
- Health Server/Client
- Generic OnOff (LED control)
- Generic Level (vibration intensity)
- Sensor Server (temperature, humidity, light)
- Time Server (RTC sync)