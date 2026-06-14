# SleepSync — KiCad Schematic Projects

Each hardware node has its own KiCad project:

| Node | KiCad Project | MCU | Key Components |
|------|--------------|-----|----------------|
| Nightstand Hub | `nightstand-hub/` | ESP32-S3-WROOM-1-N8R8 | UDA1334A, SPH0645, BME280, SCD40, TSL2591, ILI9488 |
| Sleep Strip | `sleep-strip/` | nRF52832-QFAA | LIS3DH, 2× HX711, 8× FSR-406, MP2692A |
| Climate Node | `climate-node/` | ESP32-C3-MINI-1 | SHT40, SH1106, TSAL6100, 2× Relay, BTA16 |
| Shade Controller | `shade-controller/` | ESP32-C3-MINI-1 | TMC2209, VEML7700, 3× PT4115 |

## Schematic Notes

### Nightstand Hub
- ESP32-S3 handles BLE mesh, WiFi, MQTT, audio engine, TFLite Micro
- UDA1334A connected via I2S (BCLK, WS, DOUT, MCLK)
- SPH0645 microphone on I2S input (DIN)
- BME280, SCD40, TSL2591 all on I2C0 (0x76, 0x62, 0x29)
- ILI9488 TFT on SPI0 (shared with microSD)
- MCP73831 LiPo charger with USB-C input
- AP2112-3.3 for 3.3V rail, AP6212-1.8 for ESP32-S3 core

### Sleep Strip
- nRF52832 runs bare-metal for deterministic BCG sampling
- 8 FSR-406 sensors split into 2 groups of 4
- Each group read by one HX711 24-bit ADC (high resolution)
- LIS3DH on I2C for 3-axis acceleration (actigraphy)
- TLV70233 ultra-low-Iq LDO (1µA ground current for battery life)
- MP2692A Qi wireless charging receiver
- 100mAh flex LiPo battery

### Climate Node
- ESP32-C3 runs ESP-IDF with PID control loops
- SHT40 high-accuracy temp/humidity (±0.2°C, ±1.8%RH)
- IR blaster: TSAL6100 driven by NPN transistor from GPIO
- Dual relay: SRD-05VDC-SL-C for heater and humidifier
- BTA16 triac for dimmable resistive heater control
- MOC3041 optocoupler for zero-crossing detection
- SH1106 128×64 OLED on I2C1

### Shade Controller
- ESP32-C3 runs stepper motor + LED control
- TMC2209 in stealthChop mode for silent operation
- NEMA 11 stepper drives roller shade mechanism
- VEML7700 ambient light sensor on I2C
- 3× PT4115 constant-current LED drivers (1A each)
- 12V WWA LED strip for dawn simulation
- Limit switches for top/bottom position detection