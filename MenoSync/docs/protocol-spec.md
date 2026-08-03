# MenoSync — Protocol Specification

## 1. Message Format

All messages use a binary framed format with CRC-16-CCITT integrity checking and AES-128-CTR encryption.

### Frame Structure

```
┌──────┬──────┬───────┬───────┬──────────┬──────────┬──────┬─────────────┬─────────┐
│ SYNC0│ SYNC1│ SRC_ID│ DST_ID│ MSG_TYPE │ SUBTYPE  │ SEQ  │ PAYLOAD_LEN │ PAYLOAD │ CRC16  │
│ 0x4D │ 0x53 │ 1 byte│ 1 byte│ 1 byte   │ 1 byte   │ 1 byte│ 1 byte      │ 0-240 B │ 2 bytes│
└──────┴──────┴───────┴───────┴──────────┴──────────┴──────┴─────────────┴─────────┘
```

- **SYNC0/SYNC1:** Fixed sync bytes `0x4D 0x53` ('M' 'S')
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
| VITALS_STREAM | 0x10 | Vitals data (Wrist Band → Hub) |
| EDA_STREAM | 0x11 | EDA data (Wrist Band → Hub) |
| IMU_STREAM | 0x12 | IMU data (Wrist Band → Hub) |
| BCG_STREAM | 0x13 | BCG data (Bed Mat → Hub) |
| SWEAT_DATA | 0x14 | Sweat data (Bed Mat → Hub) |
| AMBIENT_DATA | 0x15 | Ambient data (Climate Node → Hub) |
| RADIANT_DATA | 0x16 | Radiant temp data (Climate Node → Hub) |
| VOICE_PROSODY | 0x17 | Voice prosody features (Hub → Cloud) |
| HOTFLASH_PRED | 0x18 | Hot flash prediction (Hub → Cloud) |
| COOLING_CMD | 0x19 | Cooling command (Hub → Climate Node) |
| SLEEP_DATA | 0x1A | Sleep data (Hub → Cloud) |
| RISK_UPDATE | 0x1B | Risk assessment update |

## 3. Telemetry Subtypes

| Subtype | Value | Source |
|---------|-------|--------|
| WRIST_BAND | 0x01 | Wrist Band node |
| BED_MAT | 0x02 | Bed Mat node |
| CLIMATE | 0x03 | Climate Node |
| HUB | 0x04 | Hub node |

## 4. Alert Types

| Alert | Value | Severity | Description |
|-------|-------|----------|-------------|
| HOTFLASH_PREDICTED | 0x01 | High | Hot flash predicted in next 15 min |
| HOTFLASH_DETECTED | 0x02 | High | Hot flash detected (physiological) |
| NIGHT_SWEAT | 0x03 | Medium | Night sweat detected |
| SLEEP_POOR | 0x04 | Medium | Sleep quality below threshold |
| MOOD_CHANGE | 0x05 | Medium | Mood change detected (voice prosody) |
| BRAIN_FOG | 0x06 | Medium | Brain fog indicators detected |
| BONE_RISK_HIGH | 0x07 | Medium | Bone health risk elevated |
| COOLING_ACTIVATED | 0x08 | Info | Pre-emptive cooling activated |
| COOLING_FAILED | 0x09 | High | Cooling command failed |
| SENSOR_OFFLINE | 0x0A | Low | Sensor disconnected |
| SENSOR_LOW_BATT | 0x0B | Low | Sensor battery < 20% |
| MEDICATION_REMINDER | 0x0C | Info | Calcium + vitamin D reminder |
| VITAL_ABNORMAL | 0x0D | High | Vital signs outside normal range |

## 5. Command Types

| Command | Value | Description |
|---------|-------|-------------|
| START_MONITORING | 0x01 | Start menopause monitoring |
| STOP_MONITORING | 0x02 | Stop monitoring |
| CAPTURE_VOICE | 0x03 | Trigger voice sample capture |
| CALIBRATE | 0x04 | Trigger sensor calibration |
| REBOOT | 0x05 | Reboot node |
| SET_CONFIG | 0x06 | Update configuration |
| HAPTIC_ALERT | 0x07 | Trigger haptic pattern |
| AUDIO_MESSAGE | 0x08 | Play audio message |
| COOLING_START | 0x09 | Start pre-emptive cooling |
| COOLING_STOP | 0x0A | Stop cooling |
| SET_SAMPLE_RATE | 0x0B | Adjust sampling rate |
| SET_HVAC_TEMP | 0x0C | Set HVAC target temperature |
| SET_SHADE_PCT | 0x0D | Set shade closure percentage |

## 6. Vitals Data Packet (BLE GATT, 10 bytes)

```
┌──────────┬──────┬───────────┬──────────┬───────────────┬──────────┬──────────┐
│ HeartRate│ SpO2 │ SkinTemp  │ HRV      │ ActivityClass │ StepsLSB │ Battery  │
│ uint8    │uint8 │ int16 cd  │ uint16 ms│ uint8         │ uint8    │ uint8    │
│ 30-200   │85-100│ ±0.01°C   │ 0-65535  │ 0-5           │          │ 0-100%   │
└──────────┴──────┴───────────┴──────────┴───────────────┴──────────┴──────────┘
```

Transmitted at 1 Hz = 10 bytes/s

## 7. EDA Data Packet (BLE GATT, 8 bytes)

```
┌───────────────┬──────────┬──────────┬──────────┬────────────┬──────────┐
│ EDA_uS        │ EDA_std  │ Tonic    │ Phasic   │ StressLvl  │ Reserved │
│ uint16        │ uint16   │ uint8    │ uint8    │ uint8      │ uint8    │
│ 0-65535 µS    │ µS       │ baseline │ event    │ 0-3        │          │
└───────────────┴──────────┴──────────┴──────────┴────────────┴──────────┘
```

Transmitted at 4 Hz = 32 bytes/s

## 8. BCG Data Packet (BLE GATT, 8 bytes)

```
┌──────────┬──────────┬────────────┬────────────┬──────────┬───────────┬──────────┐
│ HR_bpm   │ BR_bpm   │ MotionLvl  │ SleepStage │ Battery  │ SignalQual│ Reserved │
│ uint8    │ uint8    │ uint8      │ uint8      │ uint8    │ uint8     │ uint16   │
│ 30-120   │ 6-30     │ 0-3        │ 0-3        │ 0-100%   │ 0-100     │          │
└──────────┴──────────┴────────────┴────────────┴──────────┴───────────┴──────────┘
```

Transmitted at 1 Hz = 8 bytes/s

## 9. Sweat Data Packet (BLE GATT, 8 bytes)

```
┌───────────┬────────────┬───────────────┬──────────────┬──────────┐
│ SweatRaw  │ SweatPct   │ NightSweatFlag│ BedTemp      │ Battery  │
│ uint16    │ uint8      │ uint8         │ int16 cd     │ uint8    │
│ 0-65535   │ 0-100      │ 0/1/2         │ ±0.01°C      │ 0-100%   │
└───────────┴────────────┴───────────────┴──────────────┴──────────┘
```

Transmitted at 0.05 Hz = 0.4 bytes/s average

## 10. Ambient Data Packet (Sub-GHz, 10 bytes)

```
┌───────────────┬──────────┬──────────┬──────────────┬──────────┬──────────┐
│ AmbientTemp   │ Humidity │ Pressure │ RadiantTemp  │ HVACState│ ShadePct │
│ int16 cd      │ uint16   │ uint16   │ int16 cd     │ uint8    │ uint8    │
│ ±0.01°C       │ %RH      │ hPa      │ ±0.01°C      │ 0-4      │ 0-100    │
└───────────────┴──────────┴──────────┴──────────────┴──────────┴──────────┘
```

Transmitted at 0.1 Hz via Sub-GHz 868 MHz

## 11. Cooling Command (Sub-GHz, Hub → Climate Node, 6 bytes)

```
┌──────────┬──────────────┬──────────┬──────────┐
│ Action   │ TargetTemp   │ HVACMode │ ShadePct │
│ uint8    │ int16 cd     │ uint8    │ uint8    │
│ 0-3      │ ±0.01°C      │ 0-4      │ 0-100    │
└──────────┴──────────────┴──────────┴──────────┘
```

Action: 0=stop, 1=start cooling, 2=set HVAC temp, 3=set shade

## 12. Voice Prosody Packet (128 bytes, Hub → Cloud via MQTT)

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