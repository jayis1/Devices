# CarSeatSync Architecture

## Safety model
CarSeatSync uses evidence fusion across five nodes. No single sensor is trusted for severe escalation. A red alert requires either:
- child still present after ignition off + caregiver separation, or
- dangerous cabin heat trend with persistent occupant evidence, or
- manual emergency trigger.

## Data flow
1. Seat and wearable nodes publish BLE packets every 2-5 seconds.
2. Vehicle Hub normalizes them into `csync_frame_t` records.
3. Edge FastAPI service mirrors local state to MQTT topics.
4. Cloud consumers score trend risk and notify mobile clients.
5. Handoff beacons close trip loops and reduce false positives.

## Reliability layers
- RP2040 local alarm loop works without Linux userspace.
- LTE backup works when home internet is unavailable.
- Handoff beacons provide out-of-vehicle confirmation instead of GPS-only guesses.
- LiFePO4 backup keeps the hub alive after battery disconnect.
