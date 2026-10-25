# Architecture

Topology, roles, data flow, safety limitations, user scenarios, and privacy are defined in the system README. RP2040 hub coordinates observations via an ESP32-C3 UART radio adapter concept; implementation must choose and validate a BLE host/controller stack. Local checklist state remains authoritative; missing observations are unknown, never passed. All units are SI; climate payload uses centi-degrees C and centi-percent RH. No actuators are in scope.
