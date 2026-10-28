# Schematic design note — Hub
Not a KiCad schematic or fabrication-ready design. ESP32-S3-WROOM-1 GPIO8 drives BC817-40 low-side buzzer stage through base resistor; GPIO9 drives LED with series resistor. USB-C 5 V input requires proper CC resistors, protection, and a regulated rail sized for Wi-Fi transients. Buzzer defaults off at reset.
