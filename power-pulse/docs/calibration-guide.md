# PowerPulse Calibration Guide

## Overview

Proper calibration is essential for accurate energy monitoring. The PowerPulse system requires two types of calibration:

1. **Zero-offset calibration**: Removes ADC offset with no load
2. **Gain calibration**: Scales readings to match known load values

## Prerequisites

- PowerPulse hub running and connected to network
- Multimeter with AC current clamp
- Known load (100W incandescent bulb recommended)
- Python 3.8+ with requests library

## Circuit Monitor Calibration

### Zero-Offset Calibration (Required)

This compensates for ADC offset and CT magnetization. Must be done with **no load** on all circuits.

1. Turn off all breakers (or unplug everything)
2. Run: `python calibrate_ct.py --hub http://powerpulse.local:8000 --all`
3. Wait for calibration to complete (~5 minutes for 16 circuits)
4. Verify: all circuits should read < 50mA with no load

### Gain Calibration (Recommended)

This matches the reported current/power to actual values using a known load.

1. Turn off all circuits except the one being calibrated
2. Connect a known load to the target circuit (e.g., 100W bulb)
3. Measure actual current with clamp meter
4. Run: `python calibrate_ct.py --hub http://powerpulse.local:8000 --circuit 2 --gain --known-watts 100 --known-voltage 120`
5. Verify: reported power should match ±2%

### Voltage Calibration

The voltage sense transformer may need gain adjustment:

1. Measure actual mains voltage with multimeter
2. Compare to hub reported voltage
3. Adjust voltage_gain parameter via API:
   ```json
   POST /devices/1/command
   {
     "type": "calibration",
     "cal_type": 2,
     "param_id": 0,
     "value": 1230000
   }
   ```

## Appliance Tag Calibration

### BL0937 Calibration

Each BL0937 chip needs individual calibration for voltage and current coefficients.

1. Plug the tag into a wall outlet
2. Connect a known load (e.g., 60W incandescent bulb) to the tag
3. Measure actual voltage and current with multimeter
4. Use the mobile app to enter calibration mode (Settings → Calibrate → select tag)
5. Enter measured voltage and current values
6. The app calculates and sends calibration parameters to the tag

### Manual Calibration via API

```bash
# Set voltage coefficient
curl -X POST http://powerpulse.local:8000/api/v1/devices/2/command \
  -H "Content-Type: application/json" \
  -d '{"type": "calibration", "cal_type": 2, "param_id": 0, "value": 220000}'

# Set current coefficient
curl -X POST http://powerpulse.local:8000/api/v1/devices/2/command \
  -H "Content-Type: application/json" \
  -d '{"type": "calibration", "cal_type": 1, "param_id": 0, "value": 1300}'
```

## Solar Node Calibration

### INA260 Calibration

The INA260 has factory calibration, but you should verify:

1. Measure actual solar panel voltage with multimeter
2. Compare to reported voltage in dashboard
3. If off by >5%, adjust voltage divider scaling

### MPPT Tuning

Default MPPT parameters work for most systems. If you have unusual panels:

- Increase `MPPT_STEP_SIZE` for faster tracking (less stable)
- Decrease `MPPT_STEP_SIZE` for slower, more stable tracking
- Adjust `MPPT_MIN_DUTY` and `MPPT_MAX_DUTY` based on your panel voltage

## Verification

After calibration, verify accuracy:

| Measurement | Tolerance | Method |
|-------------|-----------|--------|
| Voltage | ±1% | Compare to multimeter |
| Current | ±2% | Compare to clamp meter |
| Power | ±3% | Compare to Kill-A-Watt meter |
| Power Factor | ±0.02 | Compare to reference meter |
| Energy (Wh) | ±5% | Accumulate over 24 hours vs utility meter |

## Troubleshooting Calibration Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Current reads high with no load | Zero offset not calibrated | Run zero-offset calibration |
| Current reads low with load | Gain too low | Run gain calibration with known load |
| Power factor always 1.0 | Voltage not connected | Check voltage transformer |
| Negative power | CT clamped backward | Flip CT orientation |
| Voltage reads 0 | Voltage sense not connected | Check transformer connection |
| Solar power always 0 | INA260 not responding | Check I2C address and wiring |