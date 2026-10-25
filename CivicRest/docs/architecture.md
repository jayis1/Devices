# Architecture and decision
A facilities worker should not make repeated blind walks to discover that soap or paper is empty. CivicRest proposes readiness advisories for small public facilities. Rejected alternatives: Commonsignal (community-room opening readiness), ParkPulse (park fountain/bin upkeep), FlowGuard (household plumbing); CivicRest instead tracks restroom supply and door-event aggregates.

Door Sentinel (ESP32-C3, reed switch), Supply Scale (ESP32-C3, load-cell ADC), and Hub (Raspberry Pi Zero 2 W BLE/Wi-Fi gateway) cooperate across physically distributed fixtures. Optional MQTT/API gives staff a checklist. No person counting, camera, microphone or identity collection.

Prototype only: board drivers are TODO, no verified PCB/schematic, no measured radio range or bench-tested firmware. No measured benefit claimed. Internet outage means no remote alert; local buffering requires unfinished firmware. Human inspection remains authoritative.
