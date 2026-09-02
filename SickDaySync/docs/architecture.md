# SickDaySync architecture

## Data flow

1. Recovery Band measures HR, HRV, SpO2, skin temp, and motion every 60 seconds.
2. Room Sentinel summarizes cough counts, CO2, PM2.5, humidity, occupancy, and disturbance score every 2 minutes.
3. Med Station records medication, hydration, and thermometer events on interaction.
4. Vent Controller receives room policy commands and returns relay/actuator status plus airflow metrics.
5. Care Hub fuses state into patient-centric risk scores and publishes recommendations to mobile/web clients.

## Power architecture

- Care Hub: 12V DC input, 5V high-current buck, 3V3 digital rail, UPS-backed modem + MCU.
- Recovery Band: single-cell LiPo, 3V3 buck/boost, charger and fuel gauge.
- Room Sentinel: USB-C 5V input, isolated 3V3 sensor rail, optional LiFePO4 backup hat.
- Med Station: 5V USB-C, protected 3V3 logic rail, 5V relay rail.
- Vent Controller: 24V HVAC-class rail, isolated 5V/3V3 logic, relay and motor domains separated.

## Room control loop

- Goal variables: CO2 < 900 ppm, RH 40-55%, quiet-hours noise < policy, pressure delta -2 to -5 Pa in isolation mode.
- Inputs: occupancy, window state, exhaust availability, outdoor weather (future), patient comfort preference.
- Outputs: window position, HEPA fan level, exhaust relay, humidifier relay, room fan speed.
