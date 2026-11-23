# Architecture and failure model

Room Sentinel samples 16 kHz mono PCM from INMP441, computes RMS over 1-second frames, discards samples, then reports only a bounded level bucket. Calibration is required to convert digital amplitude to acoustic pressure; current output is indicative only. The doorway beacon is an explicit user control, not an occupancy detector. The Raspberry Pi hub owns schedules and local history.

Current implemented slice: Room Sentinel estimator core and FastAPI in-memory event endpoint. Intended future path is BLE → gateway bridge → authenticated localhost MQTT → persistent API; BLE GATT, bridge, broker integration, persistent storage, dashboard, and React Native pairing are not implemented. Internet is not needed for the design intent, but full offline operation has not been tested. A lost sensor should be marked stale after 30 seconds (policy not implemented). The API event buffer is volatile and bounded; it is not a durable history. Export/deletion UI is not implemented.

Privacy: no raw audio buffers are persisted, streamed, or exposed. Do not enable diagnostic audio logging. Users can disable any sensor and delete retained measurements. Household members must agree on installation and quiet schedules. BLE provisioning/key rotation is prototype work not complete; deploy on a trusted home network only.

Safety: low-voltage electronics only, no actuator or mains control. BLE coexistence, acoustic calibration, EMC, enclosure thermal behavior, and battery transport/safety require engineering review. LiPo charging must use a protected cell and a certified charger module; never charge an unattended damaged cell.
