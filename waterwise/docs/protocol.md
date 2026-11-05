# WaterWise interface specification

JSON object: `v` integer (1), `node_id` stable provisioned identifier, `seq` uint32 monotonic per node, `ts` UTC epoch seconds, `kind` (`flow` or `pipe_temp`), `values` numeric map, `unit` explicit per value. Reject unknown version, non-finite values, oversized payloads (>512 bytes), malformed IDs, and out-of-range physical values. BLE link-layer security/provisioning is not implemented in this prototype; do not deploy to untrusted environments. Hub discards duplicates and marks delayed readings stale; clock quality is not guaranteed.

Example: `{"v":1,"node_id":"flow-01","seq":18,"ts":1730000000,"kind":"flow","values":{"rate":4.2},"unit":{"rate":"L/min"}}`

Flow node proposed pins (ESP32-C3 DevKitM-1): GPIO4 sensor UART RX, GPIO5 TX, GPIO8 status LED; use UART at 3.3 V only. Temperature node proposed board: nRF52840 DK, GPIO P0.06 1-Wire data with 4.7k pull-up to 3.3 V; probes share bus. Verify board variant before wiring. USB power only; do not connect to mains or plumbing electrically.