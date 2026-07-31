# BloomSync — Protocol Specification

## 1. Message Format

All messages use a binary framed format with CRC-16-CCITT integrity checking and AES-128-CTR encryption.

### Frame Structure

```
┌──────┬──────┬───────┬───────┬──────────┬──────────┬──────┬─────────────┬─────────┐
│ SYNC0│ SYNC1│ SRC_ID│ DST_ID│ MSG_TYPE │ SUBTYPE  │ SEQ  │ PAYLOAD_LEN │ PAYLOAD │ CRC16  │
│ 0x42 │ 0x53 │ 1 byte│ 1 byte│ 1 byte   │ 1 byte   │ 1 byte│ 1 byte      │ 0-240 B │ 2 bytes│
└──────┴──────┴───────┴───────┴──────────┴──────────┴──────┴─────────────┴─────────┘
```

- **SYNC0/SYNC1:** Fixed sync bytes `0x42 0x53` ('B' 'S')
- **SRC_ID:** Source node ID (1-255, 0xFF = broadcast)
- **DST_ID:** Destination node ID (0xFF = broadcast)
- **MSG_TYPE:** Message type (see below)
- **SUBTYPE:** Message subtype
- **SEQ:** Sequence number (0-255, wraps)
- **PAYLOAD_LEN:** Payload length in bytes (0-240)
- **PAYLOAD:** Variable length payload
- **CRC16:** CRC-16-CCITT over header + payload (poly 0x1021, init 0xFFFF)

### Total Message Size
- Minimum: 10 bytes (header + CRC, no payload)
- Maximum: 258 bytes (header + 240 payload + CRC)

## 2. Message Types

| Type | Value | Description |
|------|-------|-------------|
| JOIN_REQ | 0x01 | Node join request |
| JOIN_ACK | 0x02 | Join acknowledgment |
| TELEMETRY | 0x03 | Periodic telemetry |
| COMMAND | 0x04 | Command from Hub to node |
| CMD_ACK | 0x05 | Command acknowledgment |
| ALERT | 0x06 | Alert notification |
| OTA_BLOCK | 0x07 | OTA firmware block |
| OTA_ACK | 0x08 | OTA block acknowledgment |
| HEARTBEAT | 0x09 | Periodic heartbeat |
| TIME_SYNC | 0x0B | Time synchronization |
| CONFIG | 0x0C | Configuration update |
| CONFIG_ACK | 0x0D | Configuration acknowledgment |
| VITALS_STREAM | 0x10 | Vitals data (Recovery Band → Hub) |
| IMU_STREAM | 0x11 | IMU data (Recovery Band → Hub) |
| NURSING_DATA | 0x12 | Nursing data (Nursing Sensor → Hub) |
| WOUND_DATA | 0x13 | Wound data (Wound Patch → Hub) |
| VOICE_PROSODY | 0x14 | Voice prosody features (Hub → Cloud) |
| RISK_UPDATE | 0x15 | Risk assessment update |

## 3. Telemetry Subtypes

| Subtype | Value | Source |
|---------|-------|--------|
| RECOVERY_BAND | 0x01 | Recovery Band node |
| NURSING_SENSOR | 0x02 | Nursing Sensor node |
| WOUND_PATCH | 0x03 | Wound Patch node |
| HUB | 0x04 | Hub node |

## 4. Alert Types

| Alert | Value | Severity | Description |
|-------|-------|----------|-------------|
| HEMORRHAGE_RISK | 0x01 | High | Hemorrhage risk elevated |
| HEMORRHAGE_HIGH | 0x02 | Critical | Hemorrhage risk critical — seek help |
| PREECLAMPSIA | 0x03 | High | Postpartum preeclampsia indicators |
| WOUND_INFECTION | 0x04 | High | Wound infection detected |
| MASTITIS | 0x05 | Medium | Mastitis risk detected |
| PPD_SCREEN_POS | 0x06 | Medium | PPD screen positive — follow-up recommended |
| VITAL_ABNORMAL | 0x07 | High | Vital signs outside normal range |
| SENSOR_OFFLINE | 0x08 | Low | Sensor disconnected |
| SENSOR_LOW_BATT | 0x09 | Low | Sensor battery < 20% |
| FEVER | 0x0A | High | Body temperature > 38.2°C |
| NURSING_REMINDER | 0x0B | Info | Time to nurse |
| MEDICATION_REMINDER | 0x0C | Info | Medication reminder |

## 5. Command Types

| Command | Value | Description |
|---------|-------|-------------|
| START_MONITORING | 0x01 | Start postpartum monitoring |
| STOP_MONITORING | 0x02 | Stop monitoring |
| CAPTURE_VOICE | 0x03 | Trigger voice sample capture |
| CALIBRATE | 0x04 | Trigger sensor calibration |
| REBOOT | 0x05 | Reboot node |
| SET_CONFIG | 0x06 | Update configuration |
| HAPTIC_ALERT | 0x07 | Trigger haptic pattern |
| AUDIO_MESSAGE | 0x08 | Play audio message |
| EMERGENCY_ALERT | 0x09 | Dispatch emergency alert |
| SET_SAMPLE_RATE | 0x0A | Adjust sampling rate |

## 6. Vitals Data Packet (BLE GATT Notification, 10 bytes)

```
┌──────────┬──────┬───────────┬──────────┬───────────────┬──────────┬──────────┐
│ HeartRate│ SpO2 │ SkinTemp  │ HRV      │ ActivityClass │ StepsLSB │ Battery  │
│ uint8    │uint8 │ int16 cd  │ uint16 ms│ uint8         │ uint8    │ uint8    │
│ 30-200   │85-100│ ±0.01°C   │ 0-65535  │ 0-5           │          │ 0-100%   │
└──────────┴──────┴───────────┴──────────┴───────────────┴──────────┴──────────┘
```

Transmitted at 1 Hz = 10 bytes/s

## 7. Nursing Data Packet (BLE GATT Notification, 10 bytes)

```
┌───────────┬────────────┬───────────┬────────────┬──────────┬──────────┐
│ TempLeft  │ TempRight  │ TempAsym  │ NursingAct │ Position │ Battery  │
│ int16 cd  │ int16 cd   │ int16 cd  │ uint8      │ uint8    │ uint8    │
│ ±0.01°C   │ ±0.01°C    │ |L-R|     │ 0/1/2      │ 0-4      │ 0-100%   │
└───────────┴────────────┴───────────┴────────────┴──────────┴──────────┘
```

Transmitted at 0.1 Hz = 1 byte/s average

## 8. Wound Data Packet (BLE GATT Notification, 10 bytes)

```
┌───────────┬──────────────┬────────────┬─────────┬────────────┬──────────┐
│ WoundTemp │ MoistureRaw  │ MoisturePct│ pHValue │ InfectionR │ Battery  │
│ int16 cd  │ uint16       │ uint8 %    │ uint8   │ uint8 %    │ uint8    │
│ ±0.01°C   │ 0-65535      │ 0-100      │ pH×10   │ 0-100      │ 0-100%   │
└───────────┴──────────────┴────────────┴─────────┴────────────┴──────────┘
```

Transmitted at 0.1 Hz = 1 byte/s average

## 9. Voice Prosody Packet (128 bytes, Hub → Cloud via MQTT)

32 float32 features extracted from 30-second voice sample:
- F0 mean, std, range, CV
- Jitter (local, PPQ5)
- Shimmer (local, APQ11)
- HNR (dB)
- Speech rate, pause ratio
- Intensity (mean, var, CV)
- Spectral (slope, flux, centroid, spread)
- MFCC coefficients 1-4
- Breathiness, roughness
- Pitch declination, voiced ratio
- Energy (mean, std)
- Duration (phoneme, pause mean, pause std)
- Composite prosody abnormality score

**No raw audio is transmitted.** Only prosody features (128 bytes) are sent.