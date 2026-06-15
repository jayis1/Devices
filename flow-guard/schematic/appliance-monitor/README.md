# FlowGuard Schematic - Appliance Monitor Node
# KiCad Project Placeholder

## MCU: nRF52832 (Low Power)

### Key Design Notes

1. **nRF52832** runs Zigbee end device + sensor polling + flow meter
2. **YF-S201** Hall-effect flow meter measures water flow through the fixture supply line
3. **BME280** provides temperature, humidity, and pressure readings for the under-sink/behind-appliance environment
4. **XGZP6847A** differential pressure sensor measures fixture-level water pressure
5. Two **stainless steel conductivity probes** detect standing water on the floor
6. **DS18B20** waterproof probe measures inlet water temperature

### Power Strategy
- 2× AA batteries (3000mAh at 3V) power the entire node
- nRF52832 sleeps between readings (System OFF mode, 0.3µA)
- YF-S201 only powered during flow detection (from GPIO pin)
- Flow detection: nRF52832 GPIO interrupt on YF-S201 pulse output wakes MCU
- BME280 polled every 60 seconds (3.6µA average)
- Conductivity probes polled every 30 seconds (GPIO excitation)
- Estimated battery life: 4+ years with normal usage patterns

### Flow Meter Integration
- YF-S201 outputs 1 pulse per 2.25 mL of water
- MCU counts pulses via GPIO interrupt (P0.02) to measure flow rate and volume
- Flow rate = pulse frequency (1 Hz ≈ 0.135 L/min)
- Maximum detectable flow: 30 L/min (YF-S201 limit)
- Accuracy: ±3% at typical residential flow rates

### Conductivity Probe Design
- Two stainless steel pad electrodes extend to floor level
- Probe pads are 15mm diameter stainless steel discs
- Pads are normally open (infinite resistance)
- When water bridges the gap, resistance drops to <100kΩ
- MCU excites P0.08 (LEAK_EXCITE) with a brief 3V pulse
- Reads P0.06 and P0.07 (LEAK_PROBE_1/2) for water presence
- De-bouncing: requires 3 consecutive positive reads (300ms total) before confirming leak
- Probes are on flexible PCB extensions (50mm long) that reach the floor

### Schematic Sheets
1. nRF52832 + debug header + antenna matching
2. YF-S201 flow meter interface (power control + pulse interrupt)
3. BME280 temp/humidity/pressure (I2C)
4. XGZP6847A differential pressure (analog)
5. DS18B20 temperature (1-Wire)
6. Conductivity probe interface (GPIO + protection)
7. Power (2× AA battery holder + LDO bypass)
8. User interface (RGB LED)

### PCB Design Notes
- 4-layer, 80×50mm
- Battery holder on bottom side (2× AA side-by-side)
- Flow meter connector on edge (3/8" compression fitting)
- Stainless steel probe pads extend from bottom edge
- Adhesive pad area on back (3M VHB)
- RGB LED on top with light pipe
- Antenna clearance zone on one end (20mm keep-out)