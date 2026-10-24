# Architecture

RepairLoop supports a volunteer-run community repair space. The hub aggregates anonymous bench occupancy, docked-tool presence and air-sentinel telemetry. Nodes have independent local indicator behavior; absence or failure of the hub does not disable equipment. Hub is RP2040 for local coordination with ESP32-C6 radio/network module. Bench and air nodes are ESP32-C6; dock uses nRF52840 BLE. Gateway bridges telemetry to local FastAPI/MQTT, optional remote sync off by default.

Failure behavior: stale node data becomes unknown after 120 seconds; sensor faults are explicitly reported; no machine is switched or interlocked by this system. Power loss loses volatile observations; retained local history is not required for operation. Time is monotonic sequence-based unless wall-clock sync is available.

Threat boundary: CRC is not authentication. Prototype must remain on a trusted test network; deployment requires per-device keys, authenticated pairing, MQTT TLS and device-scoped ACLs. Cloud loss leaves local aggregation intact. No raw audio/video or personal identity data.

Each node provides distinct information: bench occupancy supports readiness, tool dock presence supports shared-equipment availability, air sentinel reports environmental context, and hub correlates/display states. Remove any node and its associated status becomes unavailable, not inferred.

Hardware documents are conceptual pin/netlist guidance, not verified schematics. Validate module pinouts, sensor voltage, enclosure, EMC and local radio rules before fabrication.
