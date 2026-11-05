# Architecture
Gateway hosts local API and durable SQLite queue. Intake and bin nodes are telemetry-only peripherals. Human staff choose all routes; sensor readings are evidence, not decisions. No cloud dependency. On outage, use paper intake and review stored queue after recovery. Remove event DB after authorized export and event close.
