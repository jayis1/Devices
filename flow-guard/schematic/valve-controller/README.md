# FlowGuard Schematic - Valve Controller Node
# KiCad Project Placeholder

## MCU: nRF52832 + DRV8871 Motor Driver

### Key Design Notes

1. **nRF52832** runs Zigbee 3.0 router + valve motor control + sensor polling
2. **DRV8871** H-bridge drives the motorized ball valve (12V DC, bidirectional)
3. Motorized ball valve is spring-return: power loss → valve closes (fail-safe)
4. Two limit switches detect valve fully open / fully closed positions
5. MPX5700DP absolute pressure sensor (0-100 PSI) monitors whole-home water pressure
6. YF-S201 Hall-effect flow meter measures aggregate flow rate
7. Self-regulating heat trace (5W/ft, 3ft = 15W) protects exposed pipes from freezing
8. 12V DC adapter primary power + 18650 Lipo backup for reporting during power loss

### Safety-Critical Design Considerations
- **Watchdog timer**: If nRF52832 firmware crashes, WDT expires → valve spring-returns to CLOSED
- **Hardware override**: Physical push-button on front panel directly drives DRV8871 (bypasses MCU)
- **Solenoid brake**: DRV8871 supports low-side slow decay braking for controlled valve movement
- **Current limiting**: DRV8871 has overcurrent protection (4A max, valve motor peaks at 1.5A)
- **Manual override**: Ball valve has external manual handle (can be turned by hand if electronics fail)
- **Power-loss behavior**: 12V loss → valve spring-closes within 3 seconds, MCU switches to Lipo to report

### Power Design
- 12V DC adapter (2A) → LM2596-5V (1.5A) → AP2112-3.3V (nRF52832)
- 18650 Lipo → AP2112-3.3V (backup MCU power)
- DRV8871 powered directly from 12V (motor drive)
- Heat trace powered from 12V via N-MOSFET (PWM controlled)
- YF-S201 powered from 5V via nRF52832 GPIO (only enabled during flow measurement)

### Schematic Sheets
1. Power supply (12V input, 5V/3.3V regulators, Lipo backup)
2. nRF52832 + Zigbee antenna
3. DRV8871 motor driver + valve connector
4. Pressure sensors (MPX5700DP, XGZP6847A)
5. Flow meter (YF-S201)
6. Temperature sensors (2× DS18B20 — pipe surface + heat trace)
7. Heat trace driver (N-MOSFET + gate driver)
8. User interface (2× buttons, RGB LED, buzzer)
9. Debug header (SWD + UART)

### Enclosure
- 130×80×50mm ABS, IP54
- PG9 cable glands for: valve motor cable, pressure sensor cable, heat trace cable, 12V power
- Front panel: 2× buttons (open/close), RGB LED diffuser, battery compartment door