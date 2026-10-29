# RainGarden architecture

Three sensing roles report independently over 2.4 GHz Wi-Fi to a local gateway. The local Linux service ingests MQTT JSON and exposes site telemetry over HTTP; a small gateway display shows latest value, age, and fault state. Cloud hosting is optional and outside the prototype. Sensor observations are advisory, local-first where possible.

| Flow | Data | Failure handling |
|---|---|---|
| Nodes → broker | telemetry v1; node, seq, UTC time, kind, numeric values | bounded retry with jitter; duplicate seq ignored |
| Gateway → local UI | last observations and age | stale after configurable 15 min; show UNKNOWN |
| API → mobile stub | site and node observations | no control surface; API loss does not affect sensing |

Provision node IDs and broker credentials individually; production credential enrollment, authenticated OTA, secure boot, and key rotation are not implemented. Do not put the broker on the public internet. Future deployment needs TLS, per-node publish ACLs, bounded queues and retention, and authenticated administration.
