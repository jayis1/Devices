# Architecture and deployment notes

Three roles: ESP32-C3 desk check-in nodes, ESP32-C3 room climate node, Raspberry Pi Zero 2 W local gateway. BLE is intended for short-range node-to-gateway transport; implementation adapter is not supplied. Gateway API currently accepts local HTTP events, useful for tests and manual integration only. App is a disconnected display stub. All essential judgments remain with staff; the system does not control computers or electrical supplies. No cloud service is required or implemented. Local API currently stores state in memory only.

Future validation: select exact board revisions, pin/aerial layouts and enclosure; confirm BLE transport and provisioning; implement persistence/retention; exercise gateway/node power loss; perform privacy/security review and accessibility testing. Prototype readiness is advisory only.
