# QuietPath architecture

## Requirements and topology
Three battery/USB acoustic measurement sentinels cover route segments, with one powered site hub as time authority, radio coordinator, local API, and optional upstream bridge. Star topology; no node relays another node. Hub remains usable without cloud. End-to-end application is advisory and non-safety-critical.

## Interfaces
Sentinel input: INMP441 24-bit I2S at 16 kHz. Firmware computes short-window A-weighting approximation and emits 10-minute Leq / peak. Reference prototype must calibrate offsets in situ; the computation and acoustic response are not IEC 61672 compliant. SX1262 module connects SPI and DIO1; use a module with 3.3V-compatible pins. Hub uses matching regional radio interface, local SQLite deployment target (demo currently in-memory), and HTTP API. No microphone samples leave the device.

## Pin and electrical notes
See device README table. ESP32-C3 module GPIO0 is a boot strap; radio reset must be pulled to the inactive level and checked at boot. GPIO8 is a strap-sensitive pin on some module variants; do not populate LED until module datasheet and boot behavior are confirmed. Proposed assignments are not a PCB netlist. All signals 3.3V only; no 5V sensor logic. Add ESD protection at exposed connectors, reverse-polarity protection, regulator thermal margin, and RF matching per radio-module vendor layout. Obtain RF/regulatory review before deployment.

## Power estimate
At 3.3V: MCU+mic active estimate 18mA, radio TX peak 120mA for 40ms, active window 150ms/600s. Ideal average from these terms is about 0.03mA plus sleep, regulator leakage, and retries; because this arithmetic excludes board leakage and real radio receive behavior, no battery life is asserted. Measure current with a power analyzer over at least 24h before sizing a pack. USB is recommended for the first field pilot.

## Failure behavior
Sensor missing/out-of-range sets invalid flag; no zero-filled quiet reading. CRC failure drops frame. Duplicate sequence is ignored. Stale data is surfaced as unknown. Clock skew greater than bounds is quarantined. Hub outage means no live map; cloud outage does not affect local view. Firmware updates and secure key enrollment are deployment responsibilities; telemetry has no control authority.

## Rejected alternatives
- Camera-based crowd/noise estimation: disproportionate privacy impact and unnecessary for sound-level context.
- Single phone-based sampling: no simultaneous route coverage and inconsistent microphone calibration.
- Automatic noise enforcement: legally and ethically inappropriate; system only supports human context.
