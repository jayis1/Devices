# DrainSync Radio Protocol Specification

## Physical/network layer

- Medium: 868 MHz FSK on SX1262 / STM32WL integrated radio
- Topology: hub-coordinated TDMA star with optional store-and-forward relay mode
- Timeslot: 40 ms standard, 120 ms critical command slot
- Max payload: 48 bytes

## Frame layout

| Byte(s) | Field | Notes |
|---------|-------|-------|
| 0 | version | protocol version |
| 1 | msg_type | telemetry, ack, command, alert, join |
| 2-3 | src | 16-bit node ID |
| 4-5 | dst | 16-bit destination |
| 6-9 | nonce | boot/session nonce |
| 10-11 | seq | incrementing sequence |
| 12-43 | payload | encrypted application payload |
| 44-45 | crc16 | CCITT |
| 46-47 | reserved | future MAC/auth expansion |

## Message types

- `0x01` telemetry
- `0x02` ack
- `0x03` command
- `0x04` alert
- `0x05` join request
- `0x06` join response
- `0x07` firmware chunk notice

## Telemetry payload classes

### Flow summary
- drain duration ms
- turbulence score
- vibration RMS
- gas index
- leak bitmask

### Trap status
- trap depth raw
- H2S ppb
- humidity %RH
- primer cycles today

### Stack status
- cleanout distance mm
- differential pressure Pa
- surge count
- battery mV

### Actuator status
- target position
- measured position
- motor current mA
- travel time ms
- fault bitfield

## Reliability

- all commands ACKed within 250 ms
- valve-close command retried up to 5 times
- idempotent command semantics via sequence number
- duplicate frames ignored if `seq` already processed

## Security

- AES-128 CTR payload encryption
- install-time network key from hub BLE commissioning
- per-boot nonce prevents keystream reuse

## Example command flow

1. hub sends `CMD_VALVE_CLOSE`
2. actuator ACKs receipt
3. actuator moves and streams status updates
4. actuator sends final result with fault bits and position confidence
5. hub records actuation event and pushes app notification
