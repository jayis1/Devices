# MobilitySync Protocol Specification

Protocol name: **MSMP** (MobilitySync Mesh Protocol)

## Transport

- primary: 868 MHz TDMA mesh for fixed nodes and walker
- short range: BLE 5.3 GATT characteristics for walker <-> wearable cues
- positioning side-channel: IEEE 802.15.4z UWB ranging frames

## Reliability

- alerts request ACK within next slot
- commands retry up to 3 times
- duplicated sequence numbers inside a 16-packet replay window are discarded
- CRC16 protects frame integrity

## Security roadmap

- provision device root key over BLE commissioning
- derive session keys per node using ECDH
- encrypt payloads with AES-CCM in production firmware
