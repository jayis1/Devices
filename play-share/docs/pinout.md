# Candidate pin schedule

All ESP32-C3-MINI-1 boards are on candidate 5 V USB carriers with 3.3 V logic. Verify exact module/carrier datasheets and board support before build. Provide EN/BOOT and UART0 TX/RX access.

| Node | Signal | GPIO | Bus/notes |
|---|---|---:|---|
| Rack | HX711 SCK | 6 | output |
| Rack | HX711 DOUT | 7 | input |
| Dock | I²C SDA | 4 | 3.3 V pull-up |
| Dock | I²C SCL | 5 | 3.3 V pull-up |
| Dock | ToF XSHUT (optional) | 6 | output; module-dependent |
| Inspection | Button | 2 | input pull-up, switch to ground |
| Inspection | RGB status LED | 8 | current-limited driver |

These assignments are provisional. They are not proof of strap compatibility for an arbitrary carrier. Confirm GPIO availability and boot behavior against the actual ESP32-C3-MINI-1 datasheet and carrier schematic; update this document and firmware together before fabrication.