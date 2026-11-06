# Reference connection notes (not KiCad files)

These are illustrative module-level pin assignments; verify board pinout and voltage levels before wiring. Use a common 3.3 V logic rail and I2C pull-ups. Actual schematic capture, ERC, PCB, and hardware validation are outstanding.

## window-sentinel
ESP32-C3 module: GPIO4 SDA, GPIO5 SCL; VEML7700 and SHT31 on 3V3/I2C; 4.7k pullups to 3V3. USB-C 5V input -> 3.3V regulator; 100nF at each module and 10uF rail bulk. UART0 GPIO20/21 programming. Sensor I2C addresses must be checked against module straps.

## shade-node
ESP32-C3 module: GPIO4/5 I2C to AS5600; GPIO6/7 H-bridge IN1/IN2; GPIO8 PWM; GPIO9/10 normally-closed travel limit inputs with 10k pullups; GPIO3 physical override input. Driver VM from fused 12V motor supply; logic 3V3. Add 100uF at driver VM, 100nF local decoupling, TVS across motor supply. Motor driver selection and stall current must be verified before assembly.

## outdoor-node
nRF52840 module: P0.26 SDA, P0.27 SCL to VEML7700 + SHT31; P0.11 rain switch (debounced, pulled up). Protected 1-cell Li-ion -> charger/power-path -> regulated 3V3. Solar panel and charger sizing requires measured duty-cycle energy budget. Keep antenna region clear and follow module layout guidance.

## gateway
Raspberry Pi Zero 2 W, 5V USB supply, BLE onboard, no custom PCB. Use a certified USB supply.
