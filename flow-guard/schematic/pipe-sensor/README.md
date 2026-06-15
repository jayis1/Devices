# FlowGuard Schematic - Pipe Sensor Node
# KiCad Project Placeholder

## MCU: nRF52832 (Ultra-Low Power)

### Key Design Notes

1. **nRF52832** in ultra-low-power mode — targets 15µA average current on CR2477 coin cell
2. **ADXL362** 3-axis accelerometer runs in motion-activation mode (6µA) and wakes MCU on activity
3. **SPH0645LM4H** MEMS microphone normally OFF — only powered during acoustic capture bursts (triggered by ADXL362 activity or scheduled sampling)
4. **DS18B20** temperature sensor on pipe surface with thermal paste for accurate reading
5. **SHT40** ambient humidity sensor — detects condensation risk on cold pipes
6. **Conductive leak trace** — adhesive copper tape along pipe, periodically excited by MCU GPIO and read back. Wet pipe surface changes resistance.

### Ultra-Low-Power Strategy
- nRF52832 sleeps in System OFF mode between samples (0.3µA)
- ADXL362 stays in motion-activation mode (6µA) — generates interrupt on vibration above threshold
- Every 60 seconds: MCU wakes, reads DS18B20 + SHT40, transmits via Zigbee, goes back to sleep
- Every 5 minutes: transmit full sensor report (temp + humidity + vibration stats)
- On ADXL362 interrupt: MCU wakes, powers on SPH0645LM4H, captures 2-second acoustic window, runs TFLite Micro classifier, sends alert if anomaly detected
- On conductive trace wet detection: MCU wakes immediately, sends URGENT leak alert
- CR2477 (1000mAh) at 15µA average = ~7.5 year battery life

### Conductive Leak Detection Design
- Two parallel copper traces (1mm spacing) run along pipe surface on adhesive tape
- MCU GPIO P0.17 drives trace 1 high (3V) for 10ms, then reads trace 2 on GPIO P0.14
- If water bridges the gap, trace 2 reads HIGH → leak detected
- Excitation is periodic (every 10 seconds) to minimize corrosion and power
- GPIO P0.14 can also be configured as interrupt for instant detection

### Schematic Sheets
1. nRF52832 + debug header + antenna matching
2. ADXL362 accelerometer (SPI)
3. SPH0645LM4H MEMS microphone (I2S)
4. DS18B20 temperature (1-Wire)
5. SHT40 humidity (I2C)
6. Conductive leak trace interface (GPIO + protection)
7. Power (CR2477 battery holder + decoupling)

### PCB Design Notes
- 4-layer, 45×30mm
- Top layer: MCU + decoupling
- Inner layers: ground plane + power plane
- Bottom layer: sensors + antenna
- Antenna: PCB trace (inverted-F) tuned for 2.4GHz
- Battery holder on top side, accessible via slide cover
- Mounting holes for zip-tie or hose clamp