# Protocol v1

Firmware codec frame: byte 0 version (1), byte 1 type, byte 2 payload length (0–96), bytes 3–6 sequence (unsigned big-endian), followed by payload. Decoder rejects unknown version, oversized payload, truncation, and trailing bytes. This host codec has no authentication, checksum, replay defense, or radio driver; it is not secure as a standalone radio protocol. BLE pairing and integrity require a separately reviewed authenticated implementation before real deployment. Sequence is diagnostic only; gateway deduplicates application actions by event and ticket IDs. Retries are idempotent and must not trigger repeated physical actions (none are defined).

API ticket category is 1–32 characters matching letters, digits, spaces and hyphens. Time is gateway-assigned Unix time; clients do not supply authoritative timestamps. MQTT topic names are documented in the system README; no MQTT client is currently implemented.
