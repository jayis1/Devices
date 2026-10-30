# SpokeSense

Status: experimental reference design. Bench-level software only; no hardware, radio, calibration, or road testing has been performed. It is not a bicycle safety system and never controls a bicycle.

## The problem

A commuter often discovers a loose wheel, worn brake pad, or soft tire only when a ride is due. SpokeSense turns a shared household or community repair stand into a guided, evidence-based maintenance check: a wheel node measures rotation/runout, a stand node measures tire pressure and brake-lever force, and a local hub turns these into service reminders. The primary user is a volunteer bike-library mechanic supporting riders who lack specialist tools. A target workflow is a 5-minute weekly check instead of a 20-minute manual hunt for symptoms (an assumption to validate, not a measured saving).

Unlike CycleGuard, which addresses riding safety, crash detection, and theft, SpokeSense focuses on stationary maintenance and repair evidence. It does not infer roadworthiness, replace inspection, or make safety decisions.

## System

```
Wheel Sensor (ESP32-C3 + IMU + magnet) -- BLE --+
                                                 +-- ESP32-S3 Hub -- Wi-Fi/MQTT/TLS -- local API
Stand Gauge (ESP32-C3 + pressure + force) -- BLE +                         | React Native app
```

The hub performs local aggregation; cloud connectivity is optional and not required for a check. A single phone can use the documented local API. If either node is absent, the report marks that measurement unavailable rather than treating it as healthy. No actuation, camera, microphone, user identity, or location is involved. No ML output is used to label a bicycle safe.

### Nodes and prototype pin plan

- Hub: ESP32-S3-DevKitC-1, powered by USB 5 V; Wi-Fi to broker, BLE central. No mains wiring.
- Wheel Sensor: ESP32-C3-DevKitM-1, USB-rechargeable 5 V supply; BMI270 I²C SDA GPIO6/SCL GPIO7, wheel magnet reed switch GPIO4 with pull-up. IMU measures wheel runout while a mechanic rotates the wheel; reed pulses provide revolution timing. Bench use only.
- Stand Gauge: ESP32-C3-DevKitM-1; BMP280 pressure sensor I²C GPIO6/GPIO7 (prototype pressure fixture required), NAU7802 load-cell ADC I²C shared bus with distinct address; force sensor bridge on ADC board. A manual test fixture measures applied lever force; not a brake performance test.

All electronics use 3.3 V logic and protected USB SELV power. Caution: chosen breakout boards may include pull-ups/regulators; verify before connecting. These are pin/net design notes, not KiCad schematics or fabrication files. The exact pressure transducer and mechanical pressure adapter remain prototype choices, not a tire-valve product design.

## Data and operation

The versioned C codec carries node ID, sequence, monotonic uptime, type, and bounded numeric samples. BLE pairing is a commissioning step; production deployments require unique credentials, authenticated encryption, and broker ACLs. The current reference codec is a portable serialization example and does not implement BLE pairing, cryptography, OTA, or MQTT credentials.

The stand gauge reports pressure in kPa and applied lever force in N; the wheel node reports IMU deviation in mg and wheel revolutions. Device IDs are random installation identifiers. Readings are retained locally only in the demo; configure deletion/retention before any deployment. The optional ML script trains a transparent threshold baseline from explicitly synthetic examples; it is not predictive maintenance evidence and must not guide safety decisions.

## Value, installation, limits

Planning target: BOM under US$100 for hub plus two nodes (excluding the repair stand, tools, fixtures, and phone); verify sourcing before purchase. Install electronics on a stable repair stand, pair nodes locally, and perform zero/reference checks using a known pressure gauge and unloaded force sensor. The system does not measure spoke tension, brake stopping distance, tire damage, or component fatigue. Use manufacturer service limits and a qualified mechanic for inspection. No claimed reduction in accidents or repair cost is supported by test data.

## Software and checks

Portable codec and tests: `cc -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/spokesense-test && /tmp/spokesense-test`. Dashboard: `python -m pip install -r software/dashboard/requirements.txt`, then `uvicorn app:app --app-dir software/dashboard --host 127.0.0.1 --port 8000`; tests: `python -m unittest discover -s software/dashboard/tests`. ML: `python software/ml-pipeline/train.py` (requires numpy). Mobile is a minimal React Native starter. SDK-specific firmware, MQTT broker integration, physical schematics/ERC, calibrated sensing, mobile build, security review, and field tests are outstanding.

Author: jayis1.
