# Scripts

Calibration is manual: tare the assembled supply scale, apply at least three known masses across its intended range, record raw counts and fitted scale/offset in a local configuration file, then repeat after any mechanical change. Do not commit calibration records containing site identifiers. No automated calibration script is supplied because HX711 wiring and mechanics are carrier-specific.

Deployment: install Mosquitto and run FastAPI on a trusted local host, restrict listener to the site LAN, configure unique credentials and TLS where supported, set a retention policy, and keep the staff paper checklist. This prototype does not configure production broker ACLs or secure OTA.
