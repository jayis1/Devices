# Architecture and operations

Observer nodes emit append-only measurements; the gateway assigns no control authority to them. Each event carries UTC Unix seconds and a per-node sequence. The gateway is the single local coordinator. Cloud synchronization is intentionally out of scope for this prototype. Outages produce missing data, not guessed values; after reconnect, bounded queued telemetry may arrive late and consumers must order by node/sequence, not arrival time.

Trust boundary: all prototype traffic is trusted-LAN only. No authentication, TLS, key lifecycle, secure boot, signed OTA, or external MQTT broker integration is implemented. Do not expose the API or broker to the internet. Retention and deletion are not implemented in this reference. Records reveal household charging patterns; restrict LAN access and remove records before sharing logs.

Power failure, Wi-Fi loss, or gateway failure has no effect on certified EVSE operation because ChargePlan has no actuator path. EVSE observer isolation and load-meter electrical safety depend on the selected, certified components and professional installation; neither is established by these notes. No physical test or regulatory review has occurred.
