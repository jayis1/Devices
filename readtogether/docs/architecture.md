# Architecture

The goal is more successful voluntary shared-reading starts, not measuring literacy outcomes. Device events are initiated by a button or tag tap. No ambient audio/video capture exists. Book labels are arbitrary UUIDs; titles/names are optional local-only metadata.

The hub is the sole local coordinator. BLE tag advertisements are hints, not proof of a person's presence; discard raw RSSI after matching a registered tag and never construct room trajectories. Dock events travel to the local API. SQLite persistence and MQTT bridging are future integration work; the current backend uses in-memory storage and is lost on restart. Hub power loss therefore loses unpersisted records. Offline node queues should cap at 32 events and expose overflow rather than silently dropping history.

Failure behavior: reader button defaults inactive; no sound or flashing without explicit interaction; lost hub means nodes show disconnected state; no remote actuation. Adults can export/delete household data. A production system needs local authentication, encrypted backups, per-household authorization, signed firmware, retention controls, threat modeling, and accessibility testing.

Build maturity: conceptual architecture and host protocol/backend examples supplied; no complete radio drivers, native schematics, PCB, production cloud, or physical validation. Estimates are not measured.
