# CleanSync Architecture

## 1. Design Goals

1. Sense actual cleaning need instead of relying on fixed schedules.
2. Keep the robot cleaning subsystem service-ready with minimal human attention.
3. Verify hygiene on critical surfaces using objective optical signals.
4. Operate locally during WAN outages.
5. Scale from apartments to large family homes.

## 2. Data Flow

1. Dirt Sentinels publish room telemetry every 60 s nominal and every 10 s during risk events.
2. The Hub computes room cleanliness scores and issues schedule recommendations.
3. The Dock Controller accepts mission preparation commands and returns execution telemetry.
4. The Surface Wand publishes pre/post-clean verification scans tied to room, surface, and user.
5. Backend services expose REST + WebSocket APIs to the mobile app and export analytics.

## 3. Room Cleanliness Score

`cleanliness_score = 100 - weighted(dust, wetness_risk, odor, days_since_clean, residue_flags)`

Weights are room-specific:

- kitchen: grease + wetness high weight
- bath: wetness + soap film + mildew risk high weight
- entry: dust + traffic high weight
- nursery: residue + verification high weight

## 4. Node Interactions

### Dirt Sentinel -> Hub
- periodic telemetry
- tamper alarms
- battery and signal-health metrics

### Hub -> Dock Controller
- mission prepare / abort / sanitize commands
- quiet-hour and occupancy constraints
- detergent target ratios by floor type

### Surface Wand -> Hub
- scan image metadata
- spectral features
- classification result and confidence
- before/after linkage for cleaning proof

## 5. Power Domains

### Hub
- 12 V input
- 5 V CM4 rail
- 3.3 V logic/radio rail
- UPS-backed standby rail

### Dirt Sentinel
- battery rail with burst current reservoir for radio TX
- switched sensor rail for PM/VOC devices

### Dock Controller
- 24 V pump rail
- 12 V auxiliary rail
- 5 V USB/logic rail
- 3.3 V MCU/sensor rail
- isolated UV driver rail with interlock

### Surface Wand
- single-cell battery power path
- 5 V camera/LED rail
- 3.3 V MCU/sensor rail
- haptic motor boost pulse rail

## 6. Security Model

- Per-household provisioning QR code
- BLE commissioning with ECDH-derived session key
- Mesh payload encryption using rotating nonce
- Signed OTA manifests verified at hub and node class level
- Role-based mobile accounts: owner, family, cleaner, caregiver

## 7. Edge/Cloud Split

### Local-only capable
- node telemetry ingest
- cleanliness scoring
- slip alerts
- robot dispatch schedule
- recent scan history

### Cloud-enhanced
- multi-home analytics
- model retraining
- shared benchmark datasets
- long-term supply forecasting
- push notification fanout

## 8. Latency Targets

- sentinel alarm to hub alert: < 2 s
- dock command to pump start: < 500 ms
- wand scan classification: < 1.2 s local
- mobile dashboard live refresh: < 1 s after MQTT ingest

## 9. Failure Modes

- WAN down: hub buffers all telemetry locally and continues schedules.
- Dock leak detected: pumps disabled, robot dispatch blocked, app and LTE alert sent.
- Wand low battery: scan capture blocked below UV/current safety threshold.
- Sentinel packet loss: stale room score decays confidence and prompts health check.
