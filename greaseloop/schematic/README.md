# GreaseLoop pin-level design notes

These are reference pin/net schedules, not KiCad schematics and not fabrication outputs. Validate module pin mux, package footprint, load-cell mechanical mounting, power integrity, and EMC against current manufacturer datasheets before board layout.

Kitchen Dock: ESP32-C3-MINI-1, 3V3 logic. HX711 AVDD/DVDD=3V3; load cell E+/E-/A+/A- to bridge excitation/differential input. DOUT=GPIO4, SCK=GPIO5. I2C SDA=GPIO6/SCL=GPIO7 (optional OLED 0x3C); reed switch=GPIO9 to GND with 10k pull-up to 3V3; momentary button=GPIO8 to GND with internal pull-up. USB 5V input only from certified isolated supply; regulator 3V3 rated >=500mA transient. Add local decoupling per datasheets. Keep antenna edge clear; no copper under antenna. Do not connect GPIO to 5V.

Storage Sentinel: nRF52840 module, 3V supply. TMP117 V+=3V, GND, SDA=GPIO26/SCL=GPIO27, ADD0=GND (0x48), 100nF bypass. Leak strip divider: 100k series from sensor electrode to ADC GPIO2 and 100k to GND; calibrate threshold and detect open/short. CR2477 feasibility unverified: measure BLE peak current and add reservoir cap only after validating regulator/source impedance. No unattended exposure to oil or heat.

BOM references are explanatory and must be reconciled with selected module footprints and manufacturing data before fabrication.
