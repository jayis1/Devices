# Deployment and calibration

Author: jayis1

1. Mount Sink Sentinel enclosures outside splash zones; verify protected SELV supply and cable strain relief.
2. Calibrate each YF-S201 pulse count against a graduated container, then store only the calibrated coefficient locally.
3. Tare each dispenser load cell with an empty, installed bottle; test low-supply display guidance without blocking manual pumping.
4. Provision node keys out of band, configure a TLS broker and topic ACLs, then enroll each node at the local hub.
5. Simulate radio loss, low battery, empty soap, and hub reboot. Confirm every UI state becomes unknown/stale and ordinary handwashing still works.

This reference design requires electrical, RF, ingress-protection, accessibility, and privacy review before a real installation.