# Power estimates (design targets, not measurements)

Gateway: regulated 5 V USB supply, ESP32-S3 active average assumed 120 mA => 2.88 Ah/day at continuous service; use a 5 V, 2 A listed supply with margin. Not battery-operated.

Climate node: 1200 mAh Li-ion pack (protected, externally certified charger); assume 8 mA average after sensor sampling + BLE advertising => 192 mAh/day and ideal 6.25 days (1200/192). Derate to about 4 days pending measured radio/sensor duty cycle and cell capacity. Include low-voltage cutoff; do not charge unattended in field.

Supply node: 2000 mAh protected Li-ion pack; assume 4 mA average => 96 mAh/day, ideal 20.8 days (2000/96), provisional 14 days with 30% reserve. HX711 excitation and optical beam are duty-cycled; verify using a current meter. Radio peak currents and regulator transient response must be validated; use a 500 mA peak-capable 3.3 V regulator and local bulk capacitance. Battery estimates are planning arithmetic only.
