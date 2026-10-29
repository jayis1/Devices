# CommonShelf design notes

## Architecture and hazards
Gateway is the local coordinator; three independent sensors/interfaces are physically separated across storage areas. BLE links terminate at gateway. Internet/cloud is optional. Failure states are stale/unknown, never silently healthy. No motorized controls, mains wiring, or recipient surveillance. Install gateway on protected SELV supply; battery protection and charging design require review. Threats: physical access, spoofed advertisements, stolen gateway, cross-site MQTT access, replay, and supply-chain updates. Prototype does not yet implement cryptographic provisioning; isolate it from production networks.

## Protocol
Reference payload is UTF-8 JSON, version `v=1`, bounded to 160 bytes, numeric values only. `node` is an installation-generated opaque ID; `seq` is uint32; `ts` UTC epoch seconds or zero if unsynchronized. `kind` enumerated: mass_g, temp_c_x100, door. BLE advertisements are untrusted until authenticated; production design must use BLE LE Secure Connections or application-layer AEAD, replay windows and rotating keys. Retries use same sequence number; gateway deduplicates `(node,seq)`. Unknown version/kind, stale sequence, malformed JSON, and out-of-range reading are rejected. No actuator commands in v1.

## API
POST `/telemetry/{site}/{node}` accepts schema-validated JSON. MQTT topic `commonshelf/{site}/{node}/telemetry` QoS 1, no retained telemetry; retained `.../availability` is advisory. Broker ACL must bind authenticated identity to one site. TLS required outside localhost. API sample has no auth and must not be exposed publicly. No recipient or household identity fields.

## Validation boundary
Pin choices, RF range, battery life, measurement accuracy, and food-handling thresholds are unverified. Hardware notes in schematic are not KiCad ERC-ready. No physical hardware or real pantry dataset was available. Host tests cover schema/basic protocol behavior only.
