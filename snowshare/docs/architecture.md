# SnowShare architecture

Roles: ramp nodes measure local contact temperature; a separate weather node records temperature and tipping-bucket events; the site gateway provides local aggregation/display; Linux service persists observations and exposes staff API. Radio is 2.4 GHz Wi-Fi; range is site-layout dependent and must be surveyed. Sample cadence: 5 minutes (proposed), event counts debounced in firmware. Latency target is best-effort under 10 seconds on a functioning LAN, not guaranteed.

```text
DS18B20/rain reed → ESP32-C3 → Wi-Fi/MQTT → site gateway → local FastAPI/SQLite → staff app
```

No actuator is controlled. Gateway buffering is a bounded implementation requirement; if connectivity is lost, UI must show last-seen age and UNKNOWN. Rain and surface temperature are contextual signals only. Site identifiers are coarse and should not encode addresses or people. SELV USB prototypes only; no battery life claim.

## Work sequence

Operator enrolls a node with unique broker credentials; nodes publish telemetry; gateway/backend validates schema and monotonic sequence; staff review observations, physically inspect, then manually mark a task complete. A completion record is explicitly a human assertion, not sensor verification.
