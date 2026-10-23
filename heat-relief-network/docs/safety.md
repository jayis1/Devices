# Safety and privacy

Prototype-only, indoor, low-voltage SELV system. No HVAC actuation, medical use, emergency dispatch, statutory occupancy determination, or life-safety function. Maintain independent, staffed heat-response procedures. Verify sensor calibration and room conditions using approved instruments. Li-ion cells require protection, correct charging, enclosure, and safe disposal; do not use damaged cells. Radio equipment requires jurisdiction-specific FCC/CE testing before sale.

No cameras, microphones, biometrics, or visitor identities. Approximate beam-crossing counts can be inaccurate and must not be used for evacuation or legal occupancy. Counts remain local; only aggregate readiness telemetry is published. Provide site notice and opt-out via manual procedures. Default retention is at most 30 days, with operator deletion on decommissioning. TLS, scoped API authorization, per-node pairing keys, signed OTA and secure boot are required before production; several remain unimplemented in this reference prototype.

Failure safe state: there are no actuators. Stale or invalid data is displayed as unavailable, never replaced with a healthy default. Staff perform manual checks when any node or gateway is offline.
