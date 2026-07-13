# CardioSync — Protocol Specification

## BLE 5.0 GATT Service

### Service UUID
`6E400001-B5A3-F393-E0A9-E50E24DCCA9E`

### Characteristics

| UUID Suffix | Name | Direction | Properties | Payload Size |
|-------------|------|-----------|------------|-------------|
| 0002 | ECG Data | Patch→Hub | Notify | 22 bytes |
| 0003 | ECG HR | Patch→Hub | Notify | 8 bytes |
| 0004 | BP Result | Cuff→Hub | Notify | 12 bytes |
| 0005 | BP Command | Hub→Cuff | Write | 4 bytes |
| 0006 | PPG HR | Ring→Hub | Notify | 8 bytes |
| 0007 | PPG HRV | Ring→Hub | Notify | 6 bytes |
| 0008 | Activity | Ring/Patch→Hub | Notify | 6 bytes |
| 0009 | Alert | Hub→All | Write | 4 bytes |
| 000A | Heartbeat | Node→Hub | Notify | 6 bytes |

## Packet Formats

### ECG Data (0x0002)
```
Offset  Size  Field
0       2     seq_num (uint16, little-endian)
2       2     sample[0] (int16, ECG value)
4       2     sample[1]
...
20      2     sample[9]
Total: 22 bytes (2 seq + 10×2 samples)
```

10 samples per notification at 250 Hz = 25 Hz notification rate (40 ms interval).

### ECG HR (0x0003)
```
Offset  Size  Field
0       2     heart_rate_bpm (uint16)
2       2     rr_interval_ms (uint16)
4       1     motion_artifact (0=clean, 1=motion)
5       1     lead_off (0=connected, 1=off)
Total: 6 bytes
```

### BP Result (0x0004)
```
Offset  Size  Field
0       2     systolic_mmhg (uint16)
2       2     diastolic_mmhg (uint16)
4       2     map_mmhg (uint16)
6       2     heart_rate_bpm (uint16)
8       1     position_ok (0/1)
9       1     quality (0-100)
Total: 10 bytes
```

### BP Command (0x0005)
```
Offset  Size  Field
0       1     command (0=cancel, 1=measure)
1       1     schedule_id (0=on-demand, 1=AM, 2=PM, 3=post-activity)
Total: 2 bytes
```

### PPG HR (0x0006)
```
Offset  Size  Field
0       2     heart_rate_bpm (uint16)
2       2     spo2_pct (uint16, 0-100)
4       2     skin_temp_c10 (int16, °C × 10)
Total: 6 bytes
```

### PPG HRV (0x0007)
```
Offset  Size  Field
0       2     rmssd_ms (uint16)
2       2     sdnn_ms (uint16)
Total: 4 bytes
```

### Activity (0x0008)
```
Offset  Size  Field
0       1     activity_class (0=rest, 1=walk, 2=run, 3=cycle, 4=sleep)
1       1     intensity (0-100)
2       2     steps (uint16)
Total: 4 bytes
```

### Alert (0x0009)
```
Offset  Size  Field
0       1     alert_type (see cs_alert_type_t)
1       1     severity (0=info, 1=warning, 2=urgent, 3=emergency)
Total: 2 bytes
```

### Heartbeat (0x000A)
```
Offset  Size  Field
0       1     battery_pct (0-100)
1       1     status_flags (bit 0: online, bit 1: fault)
2       2     rssi_dbm (int16)
Total: 4 bytes
```

## MQTT Topics

| Topic | Direction | Payload | Description |
|-------|-----------|---------|-------------|
| `cardiosync/{uid}/hub/telemetry` | Hub→Cloud | JSON | Hub status, battery |
| `cardiosync/{uid}/hub/ecg` | Hub→Cloud | Binary (compressed) | ECG stream 250 Hz |
| `cardiosync/{uid}/hub/events` | Hub→Cloud | JSON | Arrhythmia events |
| `cardiosync/{uid}/hub/bp` | Hub→Cloud | JSON | BP records |
| `cardiosync/{uid}/hub/ppg` | Hub→Cloud | JSON | PPG summary |
| `cardiosync/{uid}/hub/alerts` | Hub→Cloud | JSON | Emergency alerts |
| `cardiosync/{uid}/cloud/config` | Cloud→Hub | JSON | Configuration updates |
| `cardiosync/{uid}/cloud/commands` | Cloud→Hub | JSON | BP schedule, OTA |

## Security

- **BLE**: LE Secure Connections (ECDH P-256), AES-128-CCM encryption
- **Cloud**: TLS 1.3, JWT authentication, AES-256 at rest
- **Firmware**: Ed25519 signed OTA updates