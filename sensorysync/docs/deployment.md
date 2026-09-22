# Deployment and recovery

Author: jayis1

1. Bench-test each board at SELV voltages only; inspect polarity and fuse operation.
2. Assign IDs from `system-manifest.json`; provision unique keys out of band.
3. Configure broker TLS and ACLs before enabling radio traffic. Do not commit `.env`.
4. Run `python3 scripts/validate.py`, then start the dashboard with a non-default token.
5. Calibrate room sound baseline in an unoccupied quiet room and label it as an estimate, not a clinical measure.
6. Verify stale telemetry, duplicate frames, radio loss, hub reboot, and local stop buttons.

Factory recovery: erase node credentials, reflash signed vendor-built firmware, and re-enrol through a local physical-presence flow. Interrupted updates must retain the prior image; this reference tree does not implement OTA bootloaders.
