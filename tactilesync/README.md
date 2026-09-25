# TactileSync — Deafblind Home Orientation & Communication

**Fidelity: reference design (unverified).** Authored by jayis1. This is an assistive-technology concept, not a life-safety, emergency-response, or medical device.

## Problem & who has it
Deafblind people can lose awareness of a visitor, an appliance finishing, a room change, or a household message when visual and audible alerts are unavailable. Existing smart-home interfaces usually assume a screen, speech, or a helper. TactileSync provides deliberate, user-owned tactile cues and a physical room model without recording speech, video, or continuous location history.

## Why this is multi-node
Orientation must work where events occur, while the wearable is personal and the hub keeps local policy and consent. Battery room anchors and appliance markers can stay in place without routing all interaction through a phone.

## System overview
Five nodes form a local BLE 5.3 + UWB household network:

```
[Room Anchor xN] --UWB ranges-- [Haptic Band]
       | BLE encrypted             |
[Door Navigator] --BLE------ [Tactile Hub] --Wi-Fi/TLS--> optional FastAPI
       | BLE                         |
[Appliance Marker xN] --------- local event cards
```

The Raspberry Pi-based **Tactile Hub** owns pairings, event rules, an encrypted local store, and an optional MQTT-over-TLS bridge. The **Haptic Band** renders only user-configured patterns. **Room Anchors** provide zone confidence, **Door Navigators** sense an open/closed threshold and render an addressable tactile route cue, and **Appliance Markers** offer a physical input and completion/maintenance event source.

## Nodes
| Node | Main silicon | Role | Power |
|---|---|---|---|
| Tactile Hub | Raspberry Pi CM4 + ESP32-C6 | local coordinator, UWB gateway, dashboard | 5 V USB-C |
| Haptic Band | nRF52840 + DRV2605L | tactile messages and acknowledge button | 180 mAh LiPo |
| Room Anchor | nRF52840 + DW3000 | UWB zone ranging and room identity | 2x AA lithium |
| Door Navigator | ESP32-C6 + DRV2605L | reed-switch doorway cue and route confirmation | 5 V USB-C |
| Appliance Marker | nRF52840 + DRV2605L | labeled tactile event/acknowledge point | CR2477 |

## Communication and privacy
BLE advertisements contain no names or event text. Commissioned connections use LE Secure Connections; application frames add a monotonically increasing sequence number and AES-CCM key selected by `key_id`. UWB ranging is used only for current room confidence; the hub stores a 15-minute rolling diagnostic window unless the user opts in to longer history. Wi-Fi traffic uses MQTT-over-TLS with certificate validation. See [docs/protocol.md](docs/protocol.md).

## Pin assignments
| Node | Signal | GPIO / bus |
|---|---|---|
| Haptic Band | DRV2605L SDA/SCL | nRF P0.26/P0.27 I²C0 |
| Haptic Band | acknowledge switch | P0.11, pull-up |
| Haptic Band | battery divider | P0.04 ADC |
| Room Anchor | DW3000 SPI SCK/MOSI/MISO/CS | P0.13/P0.15/P0.14/P0.12 |
| Room Anchor | DW3000 IRQ/RST | P0.24/P0.25 |
| Door Navigator | reed switch | ESP GPIO4, pull-up |
| Door Navigator | DRV2605L SDA/SCL | ESP GPIO6/GPIO7 |
| Appliance Marker | capacitive/tact switch | P0.11 |
| Hub | ESP32-C6 UART to CM4 | GPIO16/17 |

## Power architecture
The hub uses a USB-C 5 V/3 A supply and a 3.3 V, 2 A buck rail. The band uses MCP73831 LiPo charging, AP2112K 3.3 V regulation, and a protected 180 mAh cell. Anchors use a TPS62743 low-IQ buck from two AA lithium cells. Coin-cell markers use a load-switched haptic driver and short pulses only. No node drives mains equipment.

## Firmware
Reference C sources in `firmware/` share a versioned frame codec. Platform HAL calls are intentionally isolated behind `board_*` functions; select the relevant Nordic/ESP-IDF board layer before flashing. No firmware is bench-tested.

## Software
`software/dashboard/main.py` is a FastAPI local API with explicit user acknowledgement and MQTT ingestion. `software/ml-pipeline/train_zone_confidence.py` trains a calibrated logistic zone-confidence model from consented, labeled UWB range samples; it creates synthetic data only when asked with `--synthetic`. `software/mobile-app/` is an Expo React Native stub that calls a configurable API URL.

## BOMs and schematics
Each node has a CSV BOM in `hardware/bom/` and a textual KiCad-compatible schematic note in `schematic/`. Validate electrical, RF, antenna, battery, enclosure, and accessibility details with qualified engineers before fabrication.

## Getting started
1. Build and electrically inspect each node; do not wear an unprotected LiPo prototype.
2. Flash a board-specific HAL around the supplied reference logic.
3. Pair nodes at the hub using a physical commissioning button.
4. Use the mobile app to define tactile patterns with the intended wearer.
5. Run `python software/dashboard/main.py` with `TACTILESYNC_MQTT_URL` configured for a TLS broker.

## Accessibility and safety
Patterns must be co-designed with the wearer, remain distinguishable under stress, and have a physical “quiet” control. Door events are advisory; users need independent emergency and evacuation arrangements. The system deliberately avoids camera, microphone, and speech-recognition dependencies.
