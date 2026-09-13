# MaintainSync architecture

## Trust zones

The CM4 hub is the policy authority. ESP32-S3 radio firmware and each node validate signed frames but cannot broaden policy. Cloud is an optional mirror: all collection, alerts, baseline scoring, and interlock decisions run locally. The mobile app obtains a scoped API token and never speaks directly to an actuator.

## Data path

Tags sample IMU at 400 Hz in 4 s windows; firmware emits RMS, crest factor, spectral-band energy, temperature, and battery rather than raw audio. Utility samples pressure at 5 Hz, flow edges continuously, and RMS current over 1 s. Filter nodes report pressure, float state, and runtime every 60 s. Wand captures an explicitly initiated asset inspection. Hub persists signed event envelopes, scores models, and emits an auditable maintenance card.

## Interlock state machine

`SAFE_OFF → ARMED` requires local key, healthy heartbeat, and configured asset. `ARMED → VERIFYING` receives a policy request. `VERIFYING → ACTUATE` requires two independent hazard signals, a fresh signed command (<30 s), and no STOP input. Any fault, stale link, reset, key removal, or STOP moves directly to `SAFE_OFF` and deasserts the dry contact. A close-valve installation must preserve manual operation and local code-required shutoff.

## Installation guidance

Use removable silicone coupling for vibration tags; do not attach to hot, rotating, or electrically unsafe surfaces. Put filter docks across the filter only with manufacturer-approved pressure taps. Asset labels identify model, install date, baseline period, and service intervals. Retain images/events locally by default for 30 days; exports are owner-initiated.
