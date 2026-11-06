# Interface and architecture

Node roles, pin assignments, failure behavior, and readiness limits are in README. BLE is local telemetry transport; gateway maps received observations to MQTT over TLS (port 8883), with per-site credentials and topic ACLs. On WAN loss, gateway queues at most 24 h then drops oldest with an explicit gap counter; local station buttons remain usable. BLE loss marks data stale after 15 min. No remote actuation is supported. Provision node IDs and gateway credentials out-of-band; rotate/revoke broker credentials per site. TLS certificate verification is mandatory. OTA is out of scope; update locally with signed vendor tooling before deployment.

Components are illustrative selections, not purchased/verified. No claims of calibrated food safety or legal compliance. Validate probe sanitation, local regulations, electrical safety, radio rules, and accessibility with site operators.
