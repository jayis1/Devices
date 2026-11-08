# BatteryLoop — household rechargeable-battery readiness

Status: reference prototype; no hardware has been built or safety-certified. Invented and maintained by [jayis1](https://github.com/jayis1).

BatteryLoop helps households avoid dead batteries in remotes, toys, sensors, and other low-voltage devices by making charge state and return-to-charger habits visible. Unlike existing home energy systems, it handles removable, individually tracked NiMH cells and charger-bay workflow—not circuits, solar, or appliance power. It never charges cells itself, estimates state of health, or makes battery-safety claims. Use only a commercial charger approved for the exact cell chemistry and size.

## System architecture

```text
[Cell tray scale + ID reader] -- BLE --> [Home gateway] -- MQTT/TLS --> [FastAPI + SQLite]
[Charger-dock observer] ----- BLE --/        |                     |
[Return button beacon] ------ BLE --/         +-- offline spool   [Expo app stub]
```

The tray node reports coarse per-slot presence and mass change; the dock observer reports only human-entered charger occupancy (no electrical connection to charger); the button lets a household member mark a cell returned. A gateway groups events into a local inventory and synchronizes when online. No cloud account is required for local prototype use. There is no battery charging, charger switching, or mains connection in this design.

## Hardware reference design

All pins below refer to the named module's exposed GPIO, not chip package pins; verify against the selected module carrier before PCB layout. 3.3 V logic throughout. Current estimates and battery life are unmeasured targets.

| Node | SoC and components | Reference signals | Supply |
|---|---|---|---|
| Home gateway | Raspberry Pi Pico W (RP2040/CYW43439), Wi-Fi; USB host computer runs gateway service | USB serial; Wi-Fi upstream | 5 V certified USB supply |
| Tray scale | ESP32-C3-MINI-1; NAU7802 load-cell ADC; 4× 1 kg bar load cells as a summed platform bridge; VL53L0X presence sensor | I2C SDA GPIO4/SCL GPIO5, shared bus; ADC differential bridge input; sensor XSHUT GPIO6 | 5 V USB input, 3.3 V regulated; bridge excitation per ADC board design |
| Dock observer | ESP32-C3-MINI-1; VL53L0X per fixed bay (prototype supports one bay); user-set return button | I2C SDA GPIO4/SCL GPIO5; button GPIO7 pull-up; BLE 5 advertising | 2×AA alkaline candidate; runtime unmeasured |
| Return beacon | nRF52810 module; debounced button + LED | button P0.11, LED P0.13; BLE advertisements | CR2032 candidate, runtime unmeasured |

Use I2C pull-ups only once per bus, local decoupling at each module, reverse-polarity protection on battery inputs, and a strain-relieved load-cell connector. Do not connect any node electrically to a charger. BLE is a local prototype transport, not an authenticated production link. Radio range is site-dependent; no range guarantee is made. Cellular-free local operation is intended, but gateway persistence must be backed up by the operator.

## Protocol contract

A telemetry record is UTF-8 JSON: `{"v":1,"node":"tray-01","seq":17,"kind":"slot_mass_g","slot":"A","value":24.6,"unit":"g","time_ms":1730000000000}`. `v` is exactly 1; `seq` is an unsigned monotonic per-node counter; `kind` is one of `slot_mass_g`, `bay_present`, `return_pressed`. `slot_mass_g` must be finite and between 0 and 500 g; `bay_present` is boolean; button value is boolean. Unknown versions, oversized packets (>512 bytes), malformed JSON and out-of-range values are rejected. Gateway deduplication key is `(node, seq)`; repeated events do not create additional inventory changes. Device time is informational; gateway receipt time is authoritative for ordering. MQTT topic: `batteryloop/<site>/events`; deployment must configure broker TLS and per-device credentials. Offline events queue locally with bounded capacity; overflow is reported, never silently represented as delivered. Prototype BLE identity and MQTT authorization are not implemented and must not be exposed to an untrusted network.

## Software, ML, and app

`software/dashboard` contains a minimal FastAPI event validator and local SQLite-backed service. `software/ml-pipeline` trains a deterministic demonstration baseline from explicitly synthetic data to illustrate return-readiness estimation; it does not predict real cell capacity or provide validated performance. `software/mobile-app` is an Expo/React Native display stub. These are reference artifacts, not a deployed service. No personal identity or usage telemetry is required.

## BOM (prototype estimate, not a quotation)

| Node | Qty | Item | Notes |
|---|---:|---|---|
| Gateway | 1 | Raspberry Pi Pico W | USB cable and certified 5 V supply separate |
| Tray | 1 | ESP32-C3-MINI-1 module | carrier and antenna keep-out per Espressif reference |
| Tray | 1 | NAU7802 breakout | verify bridge excitation and input range |
| Tray | 4 | 1 kg bar load cells | mechanical platform and corner calibration required |
| Tray | 1 | VL53L0X breakout | one presence zone; optical performance depends on enclosure |
| Dock | 1 | ESP32-C3-MINI-1 module | BLE 5 |
| Dock | 1 | VL53L0X breakout | one monitored bay prototype |
| Dock | 2 | AA alkaline cells + holder | candidate supply; runtime unmeasured |
| Return beacon | 1 | nRF52810 module | confirm module supply and antenna variant |
| Return beacon | 1 | CR2032 + holder | candidate supply; runtime unmeasured |
| All | 1 set/node | 100 nF + 10 µF decoupling, connectors, enclosure | values/layout require board review |

## Validation and limitations

Firmware in `firmware/` is portable C protocol-validation code that can be host-compiled; it is not complete ESP-IDF or nRF5 target firmware. The backend and tests are executable locally. Schematics are explicit textual connection plans, not KiCad files or fabrication-ready designs. The app is a stub. Electrical design, mechanical scale calibration, radio coexistence, security, target builds, and real-device behavior are unverified. There is no validated ML dataset, measured performance, charging control, or battery-health diagnosis.

Safe behavior is passive: sensor or network failure must never affect charging, and an operator should use the charger's own status indicators. Any future installation requires an electrically reviewed board, enclosure and load calibration, secure provisioning, authenticated transport, and testing with the intended charger and cell types.
