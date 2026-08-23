# PeriodSync Protocol Specification

## Frame structure

| Byte(s) | Field |
|---------|-------|
| 0 | version |
| 1 | message_type |
| 2-3 | source_node_id |
| 4-5 | destination_node_id |
| 6-9 | unix_time |
| 10 | flags |
| 11 | payload_length |
| 12-59 | payload |
| 60-61 | CRC16-CCITT |

## Message types

| ID | Name | Purpose |
|----|------|---------|
| 0x01 | TELEMETRY | summarized sensor metrics |
| 0x02 | ALERT | leak risk, overtemp, low battery |
| 0x03 | COMMAND | relief actuation or config update |
| 0x04 | ACK | command acknowledgment |
| 0x05 | BULK_SYNC | batched cached payloads |

## Telemetry payload conventions

### TempPatch
- byte 0-1: skin temp centi-deg C
- byte 2-3: HR bpm x10
- byte 4-5: HRV RMSSD x10
- byte 6: sleep disruption count
- byte 7: pain button count

### FlowClip
- byte 0-1: capacitance delta
- byte 2: humidity RH
- byte 3: posture enum
- byte 4: leak risk score 0-100

### Relief Belt
- byte 0: mode
- byte 1-2: left temp centi-deg C
- byte 3-4: right temp centi-deg C
- byte 5: battery pct
- byte 6: fault flags

### Strip Reader
- byte 0: assay type
- byte 1-2: normalized intensity
- byte 3: validity flag
- byte 4: image reference index

## Retry strategy

- alerts: 3 immediate retries
- telemetry: next TDMA window retry once, then cache
- commands: must receive ACK within 1 second or escalate to BLE fallback if available
