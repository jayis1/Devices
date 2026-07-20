# CO2 Trap Node — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────────────┐
│             ESP32-S3-WROOM-1-N8R2                    │
│                                                     │
│  GPIO4  ── SX1262 DIO1                             │
│  GPIO5  ── SX1262 BUSY                             │
│  GPIO6  ── SX1262 NSS                              │
│  GPIO7  ── SX1262 RST                              │
│  GPIO8-10 ── SX1262 SPI                            │
│  GPIO11 ── BME280 SDA                             │
│  GPIO12 ── BME280 SCL                             │
│  GPIO13-16 ── OV2640 D7-D4 (parallel camera)     │
│  GPIO17 ── OV2640 VSYNC                           │
│  GPIO18 ── OV2640 HREF                            │
│  GPIO19 ── OV2640 PCLK                            │
│  GPIO20 ── OV2640 XCLK (20 MHz)                    │
│  GPIO21 ── OV2640 SIOC (SCCB I²C)                  │
│  GPIO26 ── OV2640 SIOD (SCCB I²C)                  │
│  GPIO33 ── IR beam break (interrupt)               │
│  GPIO34 ── Rain gauge pulse (interrupt)            │
│  GPIO35 ── Propane valve relay                    │
│  GPIO36 ── Fan PWM (LEDC)                         │
│  GPIO37 ── PTC heater PWM (LEDC)                  │
│  GPIO38 ── Battery voltage (ADC)                  │
│  GPIO39 ── Solar voltage (ADC)                    │
│  GPIO40 ── Trap full reed switch                  │
└─────────────────────────────────────────────────────┘
```

## Subcircuits

### CO2 Generation (Propane Catalytic Converter)
- Propane tank (1 lb) → regulator → catalytic converter
- Catalytic combustion: C3H8 + 5O2 → 3CO2 + 4H2O + heat
- Output: CO2 (~300 mL/min, mimics human exhalation) + 37°C surface heat
- Valve control: GPIO35 → relay → propane solenoid
- Safety: MQ-4 methane sensor (propane leak detection)

### PTC Thermistor Heater (37°C Body Temperature Mimic)
- Part: PTC thermistor 12V 5W, self-regulating at ~50°C
- PWM control: GPIO37 → LEDC PWM → MOSFET → PTC
- PID control: maintain 37°C (human body temp)
- Overheat shutoff: >70°C → firmware cuts PWM

### Suction Fan (80mm axial)
- 12V DC, draws mosquitoes into catch bag via airflow
- PWM speed control: GPIO36 → LEDC → MOSFET → fan
- Default: 80% duty → full 100% in high-risk mode

### IR Beam Break Counter
- TCRT5000 reflective optical sensor
- Mounted at trap entry funnel
- GPIO33 → interrupt (falling edge)
- Debounce: 200 ms (prevents double-counting)

### OV2640 Camera
- 2MP, 1600×1200, downsampled to 160×120 for CaptureCount CNN
- Parallel DVP interface (D4-D7, VSYNC, HREF, PCLK, XCLK)
- SCCB I²C for configuration (SIOC=GPIO21, SIOD=GPIO26)
- XCLK: 20 MHz from ESP32 LEDC
- Capture: every 15 min → upload to cloud for CaptureCount

### BME280 + Rain Gauge
- BME280: I²C 0x76, outdoor temp/humidity/pressure
- Rain gauge: tipping bucket 0.2 mm, GPIO34 interrupt
- Rain data: breeding site prediction (7–14 day lag)

### Power
- Solar: 10W 6V monocrystalline panel
- Battery: LiFePO4 3.2V 5000 mAh (extended autonomy)
- MCP73871: solar charger + battery management
- 12V boost converter: for fan + PTC heater
- AMS1117-3.3: 3.3V for ESP32 + sensors

## Safety Interlocks
1. MQ-4 propane leak sensor → immediate valve close + fan on (disperse)
2. Overheat >70°C → PTC + propane off
3. Trap bag full (reed switch) → fan off
4. Rain >10 mm/h → camera lens shutter closes
5. Wind >15 m/s → fan speed reduced
6. TPL5010 watchdog → independent reset

## PCB Layout Notes
- 4-layer board (80×80 mm)
- High-current paths (fan, PTC, propane relay) on separate plane
- Keep camera parallel bus traces equal-length
- MQ-4 gas sensor: ventilation hole in enclosure
- IP65 NEMA 4X enclosure, post-mounted