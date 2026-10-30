# Bed probe reference wiring

Not a KiCad schematic. ESP32-C3 GPIO0 ADC to capacitive probe AO (max 3.3 V); GPIO4 to DS18B20 DQ with 4.7 kΩ pull-up to 3.3 V; GPIO5 ADC via 100 kΩ/100 kΩ protected-supply divider. Common ground and local decoupling. Verify probe output, board GPIO/ADC details, cable transients and exact modules before assembly.
