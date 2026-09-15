# WorkshopSync calibration and deployment

Author: jayis1

1. With all machinery unplugged, verify each board's input voltage, fuse, reverse-polarity behavior, and no-load current.
2. Calibrate each Bench Mat zone empty; record only local offsets. Do not use e-stop input to prove or replace an emergency-stop circuit.
3. Run the Air Sentinel outdoors/clean air long enough to stabilize; compare with a known reference only as an engineering check, not a certification.
4. Attach a Tool Dock only with the tool unpowered. Verify it observes a guard/contact transition and never has an electrical path to tool power.
5. Collect at least seven days of representative, non-hazardous baseline telemetry before enabling anomaly cards. Review every card manually.
6. Test radio loss, stale data, duplicate sequences, hub restart, and expired command rejection. Confirm all outcomes are passive/advisory.
7. Use `python3 scripts/validate.py`, then deploy the dashboard with a fresh local token and device-scoped MQTT credentials managed outside Git.
