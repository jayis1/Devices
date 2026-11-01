# Architecture

Topology: three ESP32-C3-MINI-1 nodes advertise compact telemetry over BLE LE to one Raspberry Pi Zero 2 W gateway. Gateway scanner validates schema, adds receive time, de-duplicates by `(node, seq)`, persists current state, and serves a localhost-bound FastAPI interface. Wi-Fi/cloud sync is not implemented. Operator views are optional and do not affect node operation.

Failure behavior: nodes report current state repeatedly; after gateway restart, latest advertisements repopulate state. Gateway marks a node stale after 15 minutes without a valid event. A missing node is shown as unknown, never as zero stock. There are no remote commands or actuators. On sensor fault, operator UI must show unavailable and request manual check. Clock is gateway receive time; node timestamps are uptime only.

Security boundary: prototype BLE advertisements and local API have no authentication or encryption. Use only on a controlled test site with network access restricted to localhost. Production deployment requires authenticated node enrollment, protected BLE transport, TLS, access controls, and retention controls; do not expose the prototype API to a network.

Power: gateway externally powered 5 V, 2 A. Each node externally powered 5 V, 1 A; node peak depends on the selected carrier and peripherals. No battery or runtime claim. Use listed SELV adapters. Physical rack, load-bearing platform, enclosure, and ingress protection require site engineering review.

Calibration: zero and span-check each assembled rack with two known masses spanning its intended operating range; record date, masses, and residual error in local maintenance log. Recheck after movement or impact. ToF dock: measure empty/full ranges across the intended bin geometry; set conservative thresholds and validate under ambient-light extremes. Neither sensor infers item identity or safety.

Prototype limitations: no native schematics, target builds, RF survey, enclosure qualification, or field testing. Sites must retain manual inventory and inspection procedures.