# HandiSync architecture and verification

## Trust boundaries
The radio network can request action but cannot make a Dock energize or an Actuator move beyond its local hard limits. Dock ARM and OFF are hardware inputs sampled every 10 ms. Actuator E-stop removes motor energy independently of firmware. Hub policy permits a command only when it has a fresh node heartbeat (<30 s), enrolled zone match, valid HMAC, nonce/sequence, and no active safety latch.

## State machines
- **Dock:** `OFF → ARMED → VERIFYING → ON_TIMED → OFF`; any overcurrent, CT-open mismatch, TMP117 >65°C, stale hub lease, OFF button, or max-duration expires to `OFF`.
- **Actuator:** `IDLE → PRECHECK → EXTEND|RETRACT → SETTLE → IDLE`; motor current > configured threshold for 150 ms or missing Hall progress for 500 ms becomes `FAULT_LATCHED`.
- **Hub:** `NORMAL → DEGRADED` for cloud loss (local only) and `SAFETY_LATCHED` after E-stop/fault until local authenticated reset.

## Installation acceptance tests
1. Measure isolation/earth continuity per licensed electrician procedure; do not energize until pass.
2. Confirm Dock output stays de-energized after reboot, radio loss, ARM-off and physical OFF.
3. Block actuator travel with a compliant test fixture: verify cutoff <150 ms and no restart without reset.
4. Send repeated/replayed command frames: verify no state change.
5. Power-cycle hub during a timed dock session: dock must fail off within its local lease (30 s).
