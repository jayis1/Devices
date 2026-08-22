# SchoolSync Protocol Specification

## Transport layers

- Primary household mesh: 868 MHz SX1262 TDMA
- Provisioning: BLE 5.3
- High-confidence ranging: UWB (DW3110)
- WAN fallback: LTE-M via Transit Beacon

## Mesh frame

See `firmware/common/protocol.h`.

### Message types

- `SS_MSG_JOIN_REQ`
- `SS_MSG_JOIN_ACK`
- `SS_MSG_HEARTBEAT`
- `SS_MSG_BACKPACK_STATE`
- `SS_MSG_LUNCH_STATUS`
- `SS_MSG_DOOR_EVENT`
- `SS_MSG_TRANSIT_EVENT`
- `SS_MSG_COMMAND`
- `SS_MSG_CONFIG`
- `SS_MSG_ALERT`

## Security

- 128-bit network key provisioned by hub during onboarding
- Session nonce rotated per boot
- CRC16 for corruption detection; higher-level nonce prevents replay

## UWB semantics

- Doorway validation succeeds when bag range crosses from <1.5 m indoor to >2.5 m outdoor within door-open window.
- Transit handoff succeeds when Transit Beacon and Backpack Tag remain co-moving for >20 s after boarding.
