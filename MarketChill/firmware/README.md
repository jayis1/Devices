# MarketChill node firmware

The probe and door files provide portable protocol-frame construction functions. Hub main provides a platform-independent threshold/freshness alarm policy. These compile as C11 modules but are not complete ESP-IDF applications: BLE, sensor drivers, flash queue, MQTT, watchdog setup and hardware provisioning remain integration work. See each node's pin details and explicit safety limitations in the system README.
