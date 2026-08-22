# UroSync Architecture

## 1. System layers

### Sensing layer
- Toilet Dock: chemistry, conductivity, uroflow, acoustic features
- Night Mat: force distribution, transfer timing, temperature asymmetry
- Bottle Tag: intake mass delta, sip timing, adherence
- Bath Sentinel: humidity, odor/VOC context, light, leaks

### Coordination layer
- Mirror Hub schedules node wake windows, distributes config, stores local event history, and renders the household UI.
- RP2040 keeps the network operating even if the CM4 application layer restarts.

### Intelligence layer
- Rule engine handles deterministic safety thresholds immediately.
- ML services rank risks and personalize interventions.
- PDF/export pipeline packages data for external review.

## 2. Data flow
1. Node collects sensor event.
2. Event is compressed into UroSync protocol frame.
3. Hub validates CRC, decrypts payload, and republishes JSON to MQTT.
4. FastAPI persists event and updates in-memory dashboard state.
5. ML inference refreshes risk overview and recommendations.
6. App, mirror UI, and caregiver clients subscribe over WebSocket.

## 3. Power architecture by node
- Hub: 12 V primary with local 5 V and 3V3 bucks + UPS cells
- Toilet Dock: 12 V isolated bathroom adapter, separate analog 3V3 island
- Night Mat: 5 V low-profile supply with optional LiFePO4 ride-through
- Bottle Tag: single-cell LiPo with charger and deep sleep logic
- Bath Sentinel: 12 V wall adapter with relay/fan path and isolated sensor rail

## 4. Reliability design
- TDMA slots prevent bathroom burst collisions during active sessions.
- Mirror Hub keeps a 72 h offline cache if internet is unavailable.
- Alert tiers are mirrored across speaker, mobile push, and optional SMS integration.
- Each node exposes a host-compilable logic core for deterministic test coverage.

## 5. Privacy boundaries
- No camera is used in private-body space.
- Microphone features are reduced to non-speech splash/stream envelopes on-edge.
- Cloud synchronization is opt-in and can exclude chemistry values while keeping adherence summaries.
