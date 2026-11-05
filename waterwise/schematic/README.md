# Architecture and schematic notes

Flow node: certified 5 V USB input -> dev board regulator -> 3.3 V logic. Proposed UART sensor module must have 3.3 V-compatible I/O; verify module datasheet before connection. Place 100 nF decoupling at board sensor connector and provide keyed 4-pin harness (5V, GND, TX, RX). Temperature tag: 2×AA -> nRF52840 board supply; DS18B20 data line with 4.7 kΩ pull-up to 3.3 V, common ground, twisted/short lead; surface probe insulated from ambient air. Hub: Pi Zero 2 W powered by 5 V certified USB supply; BLE onboard.

These are connection-level design notes, not schematics or validated electrical drawings. No native KiCad project is supplied. Before fabrication, select exact sensor module/board revision, check absolute maximum ratings and pin multiplexing, add ESD protection and enclosure/strain relief, then create and ERC-check CAD. Avoid wet locations and ensure any installation is removable without plumbing modification.
