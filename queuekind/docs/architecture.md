# Architecture and design notes

QueueKind is a low-voltage, local-first queue estimation reference system. It measures aggregate doorway arrival counts and staff-declared service completions. It does not infer queue membership or identify visitors.

## Candidate differentiation matrix

Compared with the repository systems, QueueKind's domain is public-service queue operations, its primary user is a walk-in visitor, its input is anonymous directional threshold events plus a staff button, its output is a non-actuating wait band, and its primary link is local BLE to a standalone hub. This differs structurally from household health, safety, environment, gardening and inventory systems; no claim is made that every existing system has been exhaustively compared feature-by-feature.

## Data path

Two IR beam receivers mounted across a doorway produce direction events locally. Door firmware debounces them and increments a monotonic event sequence. The desk button node reports explicit service-complete, pause, and resume events; it does not automatically infer staff activity. Hub accepts paired-node events, validates length/version/CRC and sequence, then maintains a short rolling window. It computes an advisory wait band from current estimated queue count and a configurable service-rate baseline. The hub serves the display/API from SQLite. Optional MQTT synchronization is off by default. When disconnected, local estimates continue; stale or missing input must result in “estimate unavailable”.

## Hardware assumptions

Hub: Raspberry Pi Zero 2 W class SBC, HDMI display, certified 5 V supply. Door and desk: ESP32-C6 development modules, USB 5 V input via board regulator, nominal 3.3 V GPIO. Proposed optical inputs use GPIO4/GPIO5 with external input conditioning/level compatibility verified against exact receiver board; LED GPIO8, setup GPIO9. Desk buttons GPIO4/GPIO5 and LED GPIO8. These are candidate mappings only: validate board schematic, boot behavior and pin mux before assembly. Unique I2C buses are not used. No mains or actuators.

## Threat and hazard boundaries

Threats include forged/replayed events, unauthorized access to the local API, and disclosure of site usage patterns. The current CRC framing is not cryptographic authentication. Before real deployment, require authenticated BLE pairing/application messages, per-node revocable credentials, authorization on every endpoint, TLS for remote MQTT, retention enforcement, and security review. No cloud or ML decision may affect service priority. Staff can pause the display physically; the system has no access-control or emergency function. A fault fails to unavailable, never a fabricated low wait.

## Power and deployment

All nodes are USB powered; no battery lifetime is claimed. Use strain relief and accessible disconnects. Do not install across accessible egress paths or obstruct door hardware. Installation and signage require site approval. No physical prototype, EMC, radio-range, or field tests have been performed.
