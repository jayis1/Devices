# PregnancySync Architecture

## Design goals

1. Catch meaningful prenatal trend changes between clinic visits.
2. Keep sensing burden low enough for everyday use.
3. Prefer local-first storage and edge inference.
4. Escalate gently until thresholds justify partner or clinician alerts.
5. Keep every node field-serviceable with replaceable batteries or USB-C charging.

## Data flow

1. Hardware nodes compress sessions into typed summaries.
2. The hub validates CRC16, device nonce, and clock skew.
3. Telemetry lands in SQLite locally and optionally syncs to PostgreSQL in cloud deployments.
4. Inference helpers compute reduced-movement, hypertensive-risk, strip anomaly, and supine-burden scores.
5. Policy combines those scores with user-entered symptoms and gestational week.
6. Mobile app and clinician export endpoints render the resulting care timeline.

## Fault containment

- Hub outage: nodes retain 7 days of summaries and retry uplink.
- Internet outage: local inference and SMS/LTE backup remain active.
- Belly band low battery: sleep pad and BP cuff still support hypertensive and rest guidance.
- Sensor disagreement: hub marks confidence degraded and requests repeat measurement.

## Security

- Per-node pre-shared commissioning keys
- AES-CTR payload encryption on Sub-GHz link
- TLS for HTTPS/MQTT
- local data export signed with report hash and clinician note block
