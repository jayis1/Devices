# RoutineSync Architecture

## 1. System goals

RoutineSync is designed as an assistive infrastructure layer for executive function. The architecture prioritizes:

1. **Low-friction interaction** over app-heavy workflow.
2. **Fast local decisions** at departure moments.
3. **Probabilistic verification** rather than brittle binary reminders.
4. **Privacy-first sensing** without cameras in default deployments.
5. **Graceful degradation** during internet loss.

## 2. Architectural tiers

### Device tier
- Object Tags continuously advertise BLE packets and participate in on-demand UWB ranging.
- Doorway Dock validates departure readiness at the physical exit point.
- Focus Beacons model room state and intervention effectiveness.

### Edge tier
- Hub performs local state estimation, routine execution, and inference.
- MQTT topics are normalized into a household event bus.
- A local feature store caches the last 30 days of routine interactions.

### Cloud tier
- Optional remote sync, encrypted backups, aggregated analytics, clinician/coach sharing, and fleet OTA distribution.
- Training jobs generate refreshed models that can be re-quantized for edge use.

## 3. Event flow

1. Tags emit BLE heartbeat with battery, motion, and last anchor data.
2. Doorway Dock requests fast UWB checks during departure windows.
3. Hub fuses mass/NFC/ranging/presence and computes an exit risk score.
4. Hub selects an intervention via the NudgeBandit policy.
5. Focus Beacon receives the cue plan and renders the lowest-friction modality.
6. Results are logged for future model updates.

## 4. State machine

### Household states
- idle
- preparing
- departure_window
- focused_block
- transition_window
- reset_window

### Item states
- unknown
- nearby
- docked
- room_seen
- in_motion
- missing_critical

## 5. Reliability strategy

- Hub maintains watchdog supervision through RP2040.
- All nodes buffer at least 128 events locally.
- UWB is only used when needed to preserve power.
- BLE telemetry continues without cloud connectivity.
- Rules engine replays queued events after reconnect.

## 6. Privacy model

- All personally sensitive routine data remains local by default.
- Cloud sync is opt-in.
- Audio is transformed to numeric event features on device.
- Device-to-cloud transport uses TLS; node payloads use app-layer encryption.
