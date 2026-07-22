# Voice Hub — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────┐
│              ESP32-S3-WROOM-1-N16R8          │
│                                             │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌───────┐ │
│  │ Wi-Fi  │ │ BLE 5.0│ │ 240MHz│ │ PSRAM │ │
│  │ 2.4GHz │ │        │ │dualcore│ │ 8MB   │ │
│  └────────┘ └────────┘ └────────┘ └───────┘ │
│                                             │
│  GPIO4  ── SX1262 DIO1 (IRQ)               │
│  GPIO5  ── SX1262 BUSY                     │
│  GPIO6  ── SX1262 NSS (SPI CS)             │
│  GPIO7  ── SX1262 RST                      │
│  GPIO8  ── SX1262 SCK (SPI)                │
│  GPIO9  ── SX1262 MISO                     │
│  GPIO10 ── SX1262 MOSI                     │
│  GPIO11 ── BME280 SDA (I²C)                │
│  GPIO12 ── BME280 SCL (I²C)                │
│  GPIO13 ── DS3231 SDA (I²C shared)         │
│  GPIO14 ── DS3231 SCL (I²C shared)         │
│  GPIO15 ── SD MOSI                         │
│  GPIO16 ── SD MISO                         │
│  GPIO17 ── SD SCK                          │
│  GPIO18 ── SD CS                            │
│  GPIO19 ── SK6812 LED                      │
│  GPIO20 ── Buzzer (PWM)                    │
│  GPIO21 ── Humidifier Relay                 │
│  GPIO43 ── USB TX (UART0)                  │
│  GPIO44 ── USB RX (UART0)                  │
└─────────────────────────────────────────────┘
```

## Subcircuits

### SX1262 Sub-GHz Radio
- SPI2 host bus at 8 MHz
- DIO1 → GPIO4 (radio IRQ, edge-triggered)
- RST → GPIO7 (active low)
- BUSY → GPIO5 (input)
- 868 MHz whip antenna via SMA connector
- TCXO: 32 MHz (optional)
- Decoupling: 100nF + 10µF on VDD_CORE, 1µF on VDD_PA

### BME280 (Temp/Humidity/Pressure)
- I²C address 0x76
- SDA=GPIO11, SCL=GPIO12
- 10k pull-ups on SDA/SCL
- Indoor ambient monitoring

### DS3231 RTC
- I²C address 0x68
- Shared SDA/SCL with BME280
- Battery: CR2032 coin cell backup
- ±2 ppm accuracy

### microSD Slot
- SPI bus: MOSI=GPIO15, MISO=GPIO16, SCK=GPIO17, CS=GPIO18
- 16 GB card for 14-day data buffering
- Card detect switch

### Status LEDs (SK6812 RGB ×3)
- Data: GPIO19 (RMT peripheral for WS2812 protocol)
- 3 LEDs: mesh status, Wi-Fi/cloud, vocal health
- Green=normal, Red=high-risk mode, Blue=OTA

### Buzzer
- GPIO20 (PWM via LEDC peripheral)
- CMT-8543S-SMT 85 dB
- Used for vocal rest reminders

### Humidifier Relay
- GPIO21 (active high)
- SRD-05VDC-SL-C relay
- Controls smart humidifier power
- Flyback diode across relay coil

### Power
- Input: USB-C 5V
- TPS25940 eFuse: 5V → overcurrent protection
- AMS1117-3.3: 5V → 3.3V LDO
- 100µF + 100nF decoupling on 3.3V

## PCB Layout Notes
- 4-layer board (50×50 mm)
- SX1262 RF section: keep antenna trace short, 50Ω impedance
- BME280: away from heat-generating components
- Decoupling caps close to each IC's VDD pins
- Relay: separate ground pour, keep away from RF section