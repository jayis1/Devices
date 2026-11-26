# Architecture and failure handling

## Roles and data flow

Two microphone sentinels are intended to provide health/level metadata over BLE to a Raspberry Pi 4 room hub. The current C programs are portable host logic only: no sampling, pairing, BLE, MQTT, buffering, ASR, audio recording, or board SDK integration exists. Caption text in the prototype is supplied to the FastAPI endpoint by an explicitly authorized operator or future local ASR adapter. The app is a polling/display stub. The Python API holds caption segments in memory and loses them on restart.

Offline operation: local API and manual typed captions are the only implemented route; internet is not needed. Hub restart or API loss means captions unavailable. Do not present partial captions as a complete transcript. Stale/faulted microphone telemetry should be displayed as unknown; this demo does not yet ingest telemetry.

## Trust and privacy

No audio bytes or personal identity fields exist in the prototype. Caption text may disclose meeting content and is sensitive; memory-only retention is limited to process lifetime but does not secure the local host. Bind to loopback until client authentication and TLS are implemented. Production requires explicit consent, physical capture indication, access control, data deletion/retention settings, authenticated device onboarding and signed updates.

## Unresolved engineering

Validate exact microphone breakout electrical design, Pico W radio SDK behavior, I2S timing/PIO implementation, RF coexistence and room coverage. Select and validate local ASR separately for languages, accents, latency and word error rate. Conduct user testing with Deaf/hard-of-hearing participants and qualified accessibility reviewers. No KiCad ERC, RF, electrical, hardware-in-loop, security or field test has been performed.
