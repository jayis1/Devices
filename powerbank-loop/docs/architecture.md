# PowerBankLoop architecture and safety case

## User and operating model

A site operator places charged, commercially certified power banks in a manually accessible cabinet. Visitors borrow/return packs under the site's existing policy; the system neither identifies them nor actuates a lock. Staff use aggregate availability and maintenance trends to replenish stock. The first prototype targets indoor, staffed sites.

## Components and data path

Each pack tag broadcasts/answers a dock-local BLE query with an opaque pack ID, coarse state-of-charge band, temperature, and sequence. Dock scans only packs physically in its own slots and reports slot occupancy, dock temperature, and per-port current/voltage observations. Gateway aggregates dock reports and sends them upstream. API validates protocol version, IDs, enumerated kinds, timestamp bounds, and payload size, then retains latest state in process memory. The mobile app reads operator-facing state; it does not pair to tags.

Network loss: dock and manual lending continue; gateway queues bounded telemetry locally (storage implementation not present). On reboot, sequence continuity must be persisted. Cloud loss never blocks the local/manual activity. Sensor fault or stale data is unknown, not available. A dock's hardware enable line must have an external pulldown and independent per-port current/thermal protection; software is not the safety mechanism.

## Threats and privacy

Threats include forged/replayed radio frames, stolen node, malicious MQTT publisher, malformed payload, site key exposure, and unauthorized inventory disclosure. Production requires unique keys, authenticated encryption, secure provisioning, broker ACLs scoped to site/node, TLS client authentication, API authorization, rate limits, and revocation. The reference frame codec has CRC only: CRC detects accidental corruption, not hostile modification. Never treat it as secure. No personal identifiers are needed. The data minimization goal is site aggregate stock and technical health only.

## Hazards and exclusions

Do not design or assemble bare-cell charger circuits from these materials. Use certified enclosed power banks and listed SELV supply modules; independently verify source current limits, USB-C port behavior, thermal sensing/fuse coordination, ventilation, creepage/clearance, and fire code. This is not an unattended charging cabinet design and gives no battery safety assurance. No mains switching, powered lock, automated pack release, emergency call, or user tracking is in scope. Specialist electrical/fire review and supervised bench tests are gates before any public pilot.

## ML boundary

The training example forecasts coarse aggregate return/borrow demand using synthetic inputs. Synthetic data tests code mechanics only. No validated forecast, accuracy, deployment model, or personal behavior inference is claimed. Human restocking decisions remain authoritative; deterministic fallback is a rolling count and staff inspection.

## Validation record

The repository validation script exercises host C codec tests, API unit tests, Python syntax, CSV structure, and a synthetic ML smoke test. It does not build MCU firmware, execute radio/BLE drivers, check PCB/ERC, validate USB-C or thermal design, or establish field performance. The schematic directory contains net/pin intent only, clearly not KiCad artifacts. See README for limitations.
