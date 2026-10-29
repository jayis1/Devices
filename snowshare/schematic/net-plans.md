# Preliminary pin and net plans (not KiCad schematics)

## Gateway
U1 ESP32-S3-WROOM-1-N8; 3V3 rail; GND common; USB 5V input to qualified 3V3 regulator. GPIO8=OLED SDA, GPIO9=OLED SCL, GPIO43/44=UART0 debug (verify chosen carrier). OLED VCC=3V3, GND=GND; add local 100nF + 10uF. USB D+/D- route per Espressif reference design; antenna keep-out per module datasheet.

## Ramp node
U1 ESP32-C3-MINI-1; GPIO4=DS18B20 DQ with 4.7k pull-up to 3V3; GPIO8=status LED through 1k to GND; programming UART0 TX/RX and EN/BOOT on accessible header. All grounds common; 100nF + 10uF at module. Confirm pins against chosen module/carrier before PCB.

## Weather node
Same MCU rail/debug treatment. GPIO4=DS18B20 DQ with 4.7k pull-up; GPIO5=reed switch to GND with pull-up to 3V3 and debounce; no 5V enters GPIO.

Power and ESD protection, connectors, footprints, complete decoupling, antenna matching, and environmental protection require detailed design review. These are conceptual net specifications, not KiCad schematic files or fabrication documents.
