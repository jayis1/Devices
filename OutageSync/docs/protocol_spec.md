# OutageSync Protocol Specification

## Physical/network layer

- Band: 863-870 MHz (regional variants supported)
- Modulation: LoRa-style or FSK profile depending hardware SKU
- Superframe: 5 seconds nominal, 500 ms alert mode
- Node classes:
  - Class H: hub / always-on coordinator
  - Class M: mains-powered routers (panel, outlet)
  - Class B: battery sensor nodes (cold tags)

## Frame structure

| Field | Bytes |
|-------|-------|
| Preamble | 4 |
| Sync | 2 |
| Length | 1 |
| Src ID | 2 |
| Dst ID | 2 |
| Msg Type | 1 |
| Seq | 2 |
| Session nonce | 4 |
| Payload | 0-56 |
| CRC16 | 2 |

## Message priorities

1. `OS_MSG_ALERT`
2. `OS_MSG_PANEL_STATUS`
3. `OS_MSG_COMMAND`
4. `OS_MSG_COLD_STATUS`
5. `OS_MSG_OUTLET_STATUS`
6. `OS_MSG_FUEL_STATUS`
7. `OS_MSG_POLICY`

## Security

- Household-shared network key
- Rotating session nonce per outage event
- AES-128 CTR payload protection
- CRC-16/CCITT integrity over header+payload
