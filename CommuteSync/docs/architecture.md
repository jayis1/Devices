# CommuteSync Architecture

## System goals

1. Prevent forgotten critical items before departure.
2. Forecast lateness early enough to change route or departure time.
3. Reduce commute burden from pollution, vibration, and unsafe conditions.
4. Detect theft, tamper, and accidental separation of bags or vehicles.
5. Preserve privacy with local inference and event-only cloud sync.

## Data flow

- Entry Dock publishes `readiness_snapshot` frames after each doorway interaction.
- Bag Tag publishes `tamper`, `motion`, `battery`, and `ranging` summaries.
- Mobility Beacon publishes route samples every 30 s in motion and every 5 min parked.
- Desk Dock publishes arrival and leave-behind summaries.
- Hub Gateway normalizes all node telemetry into MQTT topics and SQLite/PostgreSQL rows.
- FastAPI serves the mobile app and workstation dashboard.

## Local-first behavior

The hub continues operating without internet:

- cached trip templates remain active;
- readiness scoring still runs;
- last-good route models remain usable;
- LTE backup is reserved for high-priority theft and crash alerts.

## Power domains

- Hub: 12V input, UPS-backed 5V/3V3 rails.
- Entry Dock: USB-C 5V, local 3V3 logic, switched 5V haptic/audio rail.
- Bag Tag: 3V coin-cell rail or LiPo + buck-boost, deep sleep outside events.
- Mobility Beacon: 12V automotive or 2S Li-ion, isolated sensor rail for SPS30 fan transients.
- Desk Dock: USB-C 5V with always-on e-ink retention.

## Security model

- ATECC608B-backed signed identity on Bag Tag.
- Per-node shared secrets for 868 MHz frames.
- TLS for MQTT and REST.
- Event-only uploads for location history by default.
- User-selectable retention windows.

## Failure handling

- Missing bag tag battery triggers doorway warning before outright failure.
- Hub RP2040 watchdog reboots CM4 if heartbeat is lost for >10 s.
- Mobility Beacon stores 24 h of route samples locally when WAN is unavailable.
- Desk Dock suppresses false missing-item alerts after positive UWB re-confirmation.
