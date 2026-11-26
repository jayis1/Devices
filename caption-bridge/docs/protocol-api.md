# API and protocol

Prototype HTTP API (FastAPI, local only):

- `GET /health` → prototype marker.
- `POST /sessions/{session}/captions` accepts JSON `{session,seq,text,final}`; session ID is 1–48 ASCII alphanumeric/underscore/hyphen characters, sequence is uint32, text 1–2000 chars. Path/body session mismatch is rejected; a sequence not greater than the process-global last accepted sequence returns 409.
- `GET /sessions/{session}/captions` returns in-memory segments for that session.

Caption payload has no speaker identity or timestamp. Restart clears state. Current ordering check is process-local and demonstration-only; no persistence or multi-client session isolation is implemented. Authentication and TLS are absent; bind to localhost/trusted LAN only.

Node telemetry v1 is bounded UTF-8 JSON `{v:1,node,seq,uptime_ms,kind:"mic_health",audio_peak,valid}`. Node IDs are bounded ASCII identifiers, seq/uptime_ms uint32, peak finite in [0,1]. Peak is relative amplitude, not sound pressure level. Wrap-safe uint32 sequence comparisons are in `firmware/common/protocol.h`; a receiver should reject duplicate/old data and enforce a monotonic freshness window. No JSON parser, radio transport, authentication, MQTT, ACK/retry or timestamp synchronization is implemented. A CRC would only detect accidental corruption, not authenticate messages.
