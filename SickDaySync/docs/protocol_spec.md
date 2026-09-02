# SickDaySync protocol spec

## Physical/link layer
- Band: 863-870 MHz regional Sub-GHz configuration
- Modulation: LoRa for alert frames, GFSK for routine telemetry
- TDMA slot: 40 ms fixed slot + 10 ms guard
- Uplink retries: 2 for routine, 5 for medical alert frames

## Frame layout
| Byte(s) | Field |
|---------|-------|
| 0-1 | source node id |
| 2-3 | destination node id |
| 4 | room id |
| 5 | frame kind |
| 6-9 | epoch seconds |
| 10 | payload length |
| 11..n | payload |
| n+1..n+2 | CRC16/CCITT |

## Common payload examples
- Recovery band: fever_tenths, spo2_pct, resting_hr, motion_index
- Room sentinel: cough_count, co2_ppm_div10, humidity_pct, occupancy_state
- Med station: dose_event, hydration_delta_ml, thermometer_present
- Vent controller: mode, pressure_pa_x10, relay_bitmap, window_pct
