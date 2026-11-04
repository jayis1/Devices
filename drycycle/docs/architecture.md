# Architecture and acceptance

Nodes: load-cell rack sensor and room temperature/relative-humidity sensor report to a local gateway. Backend storage and mobile interface are optional. The first demonstrator is a 30-minute logged mass-loss series, combined with room conditions, shown in a local UI. Acceptance is functional telemetry integrity (valid sample, dedupe, sensor-loss status), not drying-time accuracy. Requirements for field validation: test multiple fabric/load types, airflow and humidity ranges; compare estimate intervals against measured dry endpoint, report MAE and coverage by household/site.

Failure policy: stale data invalidates estimates; sensor faults are surfaced rather than interpolated silently. No remote commands or physical actuation. Power-loss recovery is best-effort and depends on persistent gateway storage. Installation is indoors, low-voltage, mechanically stable and not load-bearing for people. No building, health, or mold-safety certification.
