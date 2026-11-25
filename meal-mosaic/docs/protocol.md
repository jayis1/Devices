# Protocol

Version 1 frame is exactly 20 bytes: 0 magic `0x4d`, 1 version `1`, 2 kind (1 serving mass g, 2 return mass g, 3 room temperature centi-degrees C), 3–6 node ID little-endian, 7–10 sequence little-endian, 11–14 uptime milliseconds little-endian, 15–16 signed value little-endian, 17 status, 18 reserved zero, 19 CRC-8/ATM (poly 0x07, init 0, no reflection, xorout 0) over bytes 0–18. RH can be a separate kind/status extension in a future version; v1 does not encode it. CRC is not security. BLE link security/enrollment is required for real deployment.

Receiver validates exact length, magic, version, kind, reserved byte, CRC, and duplicate sequence. Sequence wrap uses unsigned 32-bit serial arithmetic in a future full gateway; current API rejects exact duplicate `(node,sequence)` only. Uptime is monotonic node time and must not be mistaken for UTC. Gateway supplies receive time. Stale threshold: 10 minutes.
