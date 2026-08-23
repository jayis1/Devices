# PeriodSync Architecture

## 1. System goals

PeriodSync is designed to reduce the daily friction of menstrual health management with:
- anticipatory rather than reactive symptom support;
- leak-risk prediction in both daytime and sleep contexts;
- practical actuation via heat/haptics instead of passive logging only;
- privacy-first edge operation; and
- longitudinal reports suitable for self-advocacy in clinical visits.

## 2. Data plane

1. Sensor nodes sample locally.
2. Nodes produce minute-level summaries and exception alerts.
3. Summaries travel over 868 MHz TDMA mesh to the hub.
4. Hub normalizes payloads, persists local state, publishes MQTT topics, and triggers local rules.
5. Backend stores timeseries, computes model outputs, and returns actionable recommendations.
6. Mobile clients subscribe over WebSocket or poll REST endpoints.

## 3. Control plane

- Hub is the OTA and configuration authority.
- Each node has immutable `hardware_id`, mutable `node_id`, and rotating session keys.
- Commissioning uses BLE with QR-bound household secret exchange.
- Relief commands require explicit user policy and are never triggered from cloud alone; the hub must approve.

## 4. Safety envelopes

### Relief Belt
- Max skin-contact target: 43.0 °C
- Session hard cap: 25 minutes active relief
- Cooldown lockout: 10 minutes after active session
- Heater cutoff if sensor delta > 1.2 °C, battery temp high, or NTC open/short detected

### FlowClip
- No exposed conductive body-contact elements
- Moisture electrodes isolated behind textile-facing layer
- Alert cadence capped to reduce anxiety loops

### Strip Reader Dock
- Controlled illumination with per-session white reference check
- “Informational only” banners for all non-FDA claims

## 5. Security model

- AES-128 CTR packet encryption on mesh
- CRC-16 transport integrity
- Signed OTA manifests
- Optional end-to-end encrypted clinical exports
- Raw data retention configurable from 0 to 90 days

## 6. Key MQTT topics

- `periodsync/{home_id}/telemetry/{node_id}`
- `periodsync/{home_id}/alerts/{node_id}`
- `periodsync/{home_id}/commands/{node_id}`
- `periodsync/{home_id}/shadow/{node_id}`
- `periodsync/{home_id}/models/{user_id}`

## 7. Pin highlights by node

### TempPatch
- SPI0: MAX86141
- I2C0: TMP117
- I2C1: DRV2605L
- INT lines: MAX86141, LIS2DW12
- ADC0: skin contact divider

### FlowClip
- I2C0: AD7746 + SHTC3 + BMA400
- GPIO: button, RGB LED
- SAADC: battery monitor

### Relief Belt
- I2C1: TMP117 #1/#2, MAX17048, DRV2605L
- TIM1 PWM: heater left MOSFET
- TIM2 PWM: heater right MOSFET
- ADC1: NTC safety channel

### Strip Reader Dock
- I2C0: AS7341 + e-paper
- DVP camera: OV2640
- GPIO: LED enable banks, tray switch, buzzer

## 8. Analytics outputs

- cycle phase confidence score
- period onset probability by next 1/3/7 days
- leak-risk probability by next 30/60/120 min
- cramp severity forecast 0-10
- heavy-bleeding episode flag
- low-iron discussion score
- intervention effectiveness score by protocol
