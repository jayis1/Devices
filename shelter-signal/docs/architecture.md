# Architecture and threat boundaries

Documented by jayis1.

Shelter nodes produce coarse condition observations; the gateway validates frames, timestamps receipt and should keep a bounded local queue while applying stale/threshold rules. Cloud service is optional for multi-site operations. Loss of cloud must not erase local events; loss of a node is marked stale. Loss of gateway prevents aggregation, so existing rider/staff reporting remains available.

Trust boundaries: sensor MCU → BLE link → gateway → TLS API/MQTT broker → authorized operator client. This repository implements a frame codec and demo HTTP endpoint only. Production needs per-node identities, authenticated commissioning, BLE bonding, TLS certificate verification, site-scoped broker ACLs, user authentication/object authorization, rate limits, audit, secure storage, signed updates and recovery. No keys are embedded. Never transmit passenger identities.

Hazard: false-clear output could mislead. Therefore the system reports evidence/age only; it must not automate safety clearance, transit control or emergency dispatch. Human inspection is mandatory. Environmental exposure, vandalism, battery leakage, RF coexistence and installation accessibility need field review. ML is advisory and nonessential.

Data lifecycle target: retain event summaries at most 30 days, configurable shorter; no raw audio/images are collected. IDs are site-scoped pseudonyms. Demo security and retention enforcement are incomplete.
