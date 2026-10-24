# Power and operating assumptions

Design estimates only; measure on prototypes before claiming service life.

| Node | Supply | Estimate / assumption | Runtime target |
|---|---|---|---|
| Hub | Certified isolated 5 V / 1 A USB supply | 0.3 A typical, 0.8 A Wi-Fi burst; no battery backup | Continuous |
| Shelf scale | 2×AA alkaline, 3.0 V nominal; regulated 3.3 V design to be selected | nRF module sleeps between BLE reports; HX711 duty-cycled; assume 0.25 mA average pending measurement | 6 months target, not demonstrated |
| Fridge sentinel | 2×AA alkaline, 3.0 V nominal | Probe sampled each 30 s; BLE report each 60 s; assume 0.2 mA average | 6 months target, not demonstrated |
| Intake dock | Certified isolated 5 V / 1 A USB supply | ESP32-C3 active at scan/confirmation; display duty-cycled | Continuous |

Battery nodes need undervoltage detection, reverse-polarity protection, and a low-battery flag; charging is intentionally absent. Do not use rechargeable Li-ion in this reference design without a separately reviewed protection/charging design. Hub and dock use certified SELV supplies only. Final regulator selection, peak current and thermal margin are unverified.
