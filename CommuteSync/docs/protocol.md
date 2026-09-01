# CommuteSync Protocol

## Radio layers

- 868 MHz TDMA mesh for deterministic coordination inside home and office
- BLE 5.x for provisioning and phone interaction
- UWB for exact proximity and handoff confirmation
- MQTT over TLS for cloud sync

## TDMA timing

- Superframe: 2 seconds
- Slot 0: hub beacon + clock sync
- Slot 1: Entry Dock
- Slot 2: Bag Tag
- Slot 3: Mobility Beacon
- Slot 4: Desk Dock
- Slot 5: retransmit / OTA
- Slots 6-7: spare / future nodes

## Common frame

| Offset | Bytes | Meaning |
|--------|-------|---------|
| 0 | 1 | preamble `0xA5` |
| 1 | 1 | protocol version |
| 2 | 1 | message type |
| 3 | 1 | source node |
| 4 | 1 | destination node |
| 5 | 1 | flags |
| 6 | 2 | payload length |
| 8 | N | payload |
| 8+N | 2 | CRC16-CCITT |

## Message catalogue

- `0x01` heartbeat
- `0x10` readiness snapshot
- `0x11` bag tamper
- `0x12` route sample
- `0x13` arrival summary
- `0x20` model score
- `0x30` intervention command
- `0x40` OTA fragment
- `0x7F` fault

## Pairing

1. Hub generates short-lived claim token.
2. Node advertises BLE commissioning service.
3. Phone transfers token over BLE.
4. Node exchanges 868 MHz challenge-response with hub.
5. Hub stores signed node identity and assigns TDMA slot.

## OTA

Firmware images are chunked into `0x40` fragments with monotonic sequence numbers. Hub retries up to three times per fragment and requires SHA-256 verification before activation.
