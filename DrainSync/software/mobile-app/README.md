# DrainSync Mobile App

React Native / Expo stub for installers and households.

## Planned screens

- home risk dashboard
- per-fixture branch health
- live stack pressure and cleanout level
- backwater valve control with confirmation flow
- service mode calibration and waveform capture
- alert history and recommended maintenance

## Data sources

- FastAPI REST `/api/v1/overview`
- MQTT push notifications via gateway relay
- BLE provisioning for first-time node install
