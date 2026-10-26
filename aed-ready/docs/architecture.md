# Architecture and failure behavior

Cabinet sentinel advertises bounded status frames; gateway associates provisioned node ID to a cabinet label and timestamps receipt. Local rules issue maintenance reminders for door events, stale heartbeat, invalid sensor flag, or a site-configured temperature excursion. Staff explicitly record physical inspection through the local facility UI. Cloud/MQTT is optional and outbound only in a future deployment.

| Failure | Detection | Response | Recovery |
|---|---|---|---|
| Sentinel battery/connection loss | No fresh advertisement | Display cabinet unknown/stale, not ready | Replace battery; physically inspect |
| Gateway power loss | Local UI unavailable | Cabinet has no remote status; ordinary AED procedures continue | Restore listed supply; review gap |
| Internet/MQTT unavailable | Uplink timeout | Local gateway continues; queueing is not implemented | Restore uplink; manually reconcile |
| Invalid/replayed frame | Parser/sequence check | Drop frame and log diagnostic | Check node/recommission |
| Temperature sensor failure | Sensor-valid flag absent | Unknown, request manual check | Replace/bench-test sensor |
| Door contact chatter | Debounce required (HAL TODO) | May create noisy advisory events | Inspect magnet/contact |
| Staff check falsely recorded | Not detectable electronically | Interface labels as staff-attested, never independently verified | Physical inspection per facility policy |

Privacy: no personal data or occupant sensing. Staff identity should not be stored by default. No camera/microphone. Firmware transport and enrollment security are prototype gaps. Local retention/access policy is deployment-owned. Temperature excursion settings must be derived from the AED manufacturer's documentation; this design supplies no universal threshold.