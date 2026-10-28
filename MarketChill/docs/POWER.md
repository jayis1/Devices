# Power assumptions (estimates, not measurements)

Hub: regulated 5 V USB supply, ESP32-S3 Wi-Fi/BLE peak current sized with at least 1 A transient headroom; design target <1 W average at 10-second telemetry interval. Use a certified isolated USB supply; no mains circuitry in this project.

Probe and door: 2×AA primary cells, 3.0 V nominal; carrier must provide a 3.3 V-compatible supply rail over the cell discharge range (use a qualified buck/boost regulator, not direct connection unless the selected board accepts the full range). BLE advertising every 10 s; sleep between events; target >6 months is an unvalidated estimate. Measure current across cell voltage and temperature before claiming runtime. Battery chemistry, protection, reverse-polarity protection, holder, and enclosure remain board-level design decisions. Do not recharge primary cells.
