# PorchGuard — Assembly Guide

## Overview

PorchGuard consists of 4 nodes. Install the **porch camera first** — it
provides the pirate detection that protects your deliveries.

## Parts List

| Node | Key Components | Est. Cost |
|------|---------------|-----------|
| Hub | RP2040 + ESP32-C6, SX1262, ILI9341 TFT, piezo siren, LiPo 2500mAh | ~$23 |
| Porch Camera | ESP32-S3 (PSRAM), SX1261, OV2640, HLK-LD2410, AM612, INMP441, MAX98357A | ~$19 |
| Mailbox | STM32L011, SX1261, HX711, 1kg load cell, DS18B20, LIS2DH12, CR2032 + solar | ~$15 |
| Lock | nRF52840, A4988 stepper, keypad, relay, LIS2DH12, 4×AA | ~$17 |

## Step 1: Hub Node

### Assembly
1. Solder all components per schematic (`schematic/hub-node/`)
2. Flash firmware: `firmware/hub-node/hub_main.c` (RP2040) + ESP32-C6 WiFi bridge
3. Connect 915MHz antenna to SMA connector
4. Insert LiPo battery (backup power — critical for siren during outage)
5. Connect USB-C power

### Placement
- Indoors near the entrance, central location
- Wall-mount or shelf (within 30m of porch camera + mailbox)
- Connect to WiFi via mobile app (BLE pairing)

### Verification
- TFT displays "PorchGuard Hub v1.0"
- Status LED: solid green (armed)
- Display shows 0 active nodes (until others are installed)

---

## Step 2: Porch Camera Node (INSTALL FIRST — Pirate Protection!)

### Assembly
1. Solder all components per schematic (`schematic/camera-node/`)
2. Flash firmware: `firmware/camera-node/camera_main.c` (ESP32-S3)
3. Mount OV2640 with 160° lens facing porch
4. Connect mmWave (HLK-LD2410) pointing at porch area
5. Connect PIR sensor (AM612) facing approach path
6. Insert MicroSD card (clip storage)
7. Wire power: doorbell transformer 16-24VAC → MP1584 buck → 5V

### Placement
- Above front door, 2.0-2.5m height
- Camera angled down ~15° to view porch + approach
- mmWave sensor perpendicular to approach path
- PIR covering the approach zone (driveway/walkway)

### Verification
- Connects to WiFi (clips upload)
- Connects to hub mesh (TFT shows 1 active node)
- Wave hand in front → presence state updates on hub display
- Place a box on porch → "DELIVERY" event on app

---

## Step 3: Mailbox Node

### Assembly
1. Solder all components per schematic (`schematic/mailbox-node/`)
2. Flash firmware: `firmware/mailbox-node/mailbox_main.c` (STM32L011)
3. Mount load cell under mail tray (mail weight rests on it)
4. Mount reed switch on mailbox door frame
5. Mount solar panel on top of mailbox (facing up/south)
6. Insert 2× CR2032 batteries
7. Connect antenna (PCB trace or SMA whip)

### Placement
- In or under mailbox at curb
- Antenna oriented for best path to hub
- Solar panel unshaded

### Verification
- Open mailbox door → hub receives "mail-arrived" event
- Place a letter → weight class updates
- Tilt mailbox → TAMPER_ALERT fires (hub + app)

---

## Step 4: Lock Node + Garage Relay

### Assembly
1. Solder all components per schematic (`schematic/lock-node/`)
2. Flash firmware: `firmware/lock-node/lock_main.c` (nRF52840)
3. Install interior escutcheon: motor + MCU + battery + relay
4. Install exterior keypad (ribbon cable through door)
5. Connect stepper motor to deadbolt (retrofit kit)
6. Wire garage relay to opener (use optocoupler isolation)
7. Insert 4× AA batteries
8. Pair with phone via BLE (app setup wizard)
9. Pair with hub via BLE (so hub can unlock remotely)

### Placement
- Interior: on inside of door, deadbolt height
- Exterior: keypad at comfortable height
- Garage relay: near opener, dry location

### Verification
- Enter master PIN → deadbolt unlocks, then auto-locks after 30s
- App unlock works (BLE)
- Garage relay fires from app
- Door-left-open >2min → app alert

---

## Step 5: Enrollment & Calibration

### Resident Enrollment
Walk past the porch camera 3 times each. The re-ID model captures your
embedding and adds you to the resident gallery. Verify in the app that
you're recognized as "Resident" not "Unknown".

### Courier Auto-Learning
After a courier visits 3+ times, the system auto-promotes them from
"Unknown" to "Courier" in the gallery. You can name them in the app.

### Calibration
Run `scripts/calibrate_sensors.py --node all`:
- Camera: white balance, mmWave zone config, PIR sensitivity
- Mailbox: load cell tare + known-weight scale, reed debounce
- Lock: motor stroke calibration, door sensor confirm

---

## Step 6: Issue Your First Courier Code

1. In the app → Codes → Issue New Code
2. Set validity window (e.g., 60 minutes)
3. Add note ("Amazon delivery")
4. Share the 6-digit code with the courier
5. Courier enters code on keypad → deadbolt unlocks + garage relay fires
6. Courier places parcel inside garage → secure, weather-protected
7. Code is single-use — cannot be reused

---

## Step 7: Arm the Porch

Toggle "Arm" in the app. The status LED on the hub turns green (armed).
Now the system actively watches for pirates 24/7.

You're protected. 🛡️