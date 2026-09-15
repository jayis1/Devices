# Air Sentinel conceptual schematic

Author: jayis1. Conceptual connection notes, not fabrication-ready KiCad.

USB-C 5 V → polyfuse/TVS → AP2112K 3.3 V. ESP32-C6 GPIO6/7 I²C connect SCD41, SPS30, and SGP40; GPIO2 receives a protected airflow pulse. SX1262 GPIO10/11/12/13/4 map NSS/MOSI/MISO/SCK/DIO1. Maintain particulate sensor airflow clearance and an RF antenna keep-out.