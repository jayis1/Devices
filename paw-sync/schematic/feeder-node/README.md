# Smart Feeder Schematic — PawSync

## MCU Architecture

Single SoC: **ESP32-C6** — WiFi6 + BLE5.3 mesh + motor control + load cell reading + RFID.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │          SMART FEEDER NODE                   │
                    │          (kitchen, plugged)                  │
                    │                                            │
  USB-C 5V ───────► │  RT9013 3.3V LDO ──── all logic            │
  LiPo 1000mAh ──►  │  Backup battery (power outage)               │
                    │                                            │
  ESP32-C6 ───────  │  GPIO    ── HX711 SCK/DOUT (load cells)     │
                    │  GPIO    ── A4988 stepper driver            │
                    │  SPI     ── MFRC522 RFID reader             │
                    │  I2C     ── SSD1306 OLED display            │
                    │  ADC     ── Capacitive water level sensor   │
                    │  GPIO    ── WS2812 status LED               │
                    │  GPIO    ── Manual feed button              │
                    │  GPIO    ── Low-food reed switch            │
                    │  BLE5.3  ── Mesh to hub + collar (RFID)     │
                    │  WiFi6   ── Cloud schedule sync              │
                    │                                            │
  Load Cells ─────► │  4× 1kg strain gauge under food bowl        │
  Stepper + Auger ► │  Precision dispensing (0.5g increments)     │
  RFID Antenna ──►  │  13.56MHz pet identification                │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-C6

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1  | HX711 SCK | load cell clock |
| GPIO2  | HX711 DOUT| load cell data |
| GPIO3  | Motor DIR | stepper direction |
| GPIO4  | Motor STEP| stepper step pulse |
| GPIO5  | Motor EN  | stepper enable |
| GPIO6  | RFID SCK  | MFRC522 SPI |
| GPIO7  | RFID MISO | MFRC522 |
| GPIO8  | RFID MOSI | MFRC522 |
| GPIO9  | RFID CS   | MFRC522 CS |
| GPIO10 | RFID RST  | MFRC522 reset |
| GPIO12 | OLED SDA  | SSD1306 I2C |
| GPIO13 | OLED SCL  | SSD1306 I2C |
| GPIO14 | Water ADC  | capacitive water level sensor |
| GPIO15 | WS2812    | status LED |
| GPIO16 | Button    | manual feed button |
| GPIO17 | Low-food SW| hopper low-food reed switch |

## Power

- USB-C 5V → RT9013 3.3V LDO → logic
- Stepper motor draws ~400mA during dispensing (from 5V rail directly)
- LiPo 1000mAh backup → continues through power outage (sans motor)