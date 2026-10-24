# Architecture and trust boundaries

PantryLink separates a physical site from optional cloud services. The site hub is the authority for current display and local buffering; cloud sync is never required for daily intake or an on-site temperature check. Shelf nodes measure mass only: volunteer-entered item class is the source of item identity. The intake dock creates an adjustment only after a volunteer confirms the displayed category and scale reading.

Node links: authenticated BLE 5 to hub. Hub uplink: HTTPS or MQTT/TLS over Wi-Fi when enabled. No cross-site peer radio or remote control. Local ring buffer is capped at 24 hours; gaps are surfaced. Cloud payloads use site-scoped pseudonymous node IDs and broad stock status. Disable cloud sync for offline-only deployments.

Trust boundaries: (1) sensor peripherals are noisy/untrusted; validate ranges and calibration; (2) BLE enrolled node identity is not implemented in this code sample and must use per-device bonding in product firmware; (3) hub stores site data locally; physical access to hub is trusted; (4) API sample has no authentication and MUST remain localhost/demo until tenant auth, TLS termination, and authorization are implemented. Never expose it directly to the internet.

Threats: spoofing, replay, lost devices, volunteer mistakes, stale readings, malicious local network, and accidental recipient-data entry. Mitigations in concept: physical enrollment window, unique credentials, bounded queues, idempotent sequence handling, TLS, least-privilege site ACLs, coarse availability, no recipient identity. Not implemented: key rotation/revocation, secure boot, signed OTA, audit identity, encrypted-at-rest persistence. A paper log remains the fallback.
