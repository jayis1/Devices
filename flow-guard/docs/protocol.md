# FlowGuard Zigbee Protocol Specification

## Network Configuration

| Parameter | Value |
|-----------|-------|
| Profile | Home Automation (0x0104) |
| Device ID | Custom (0xFC00) |
| Channel | 25 (2425 MHz) |
| PAN ID | 0xF60D |
| Network Key | AES-128, installed during commissioning |
| Link Keys | Per-device, derived from Install Code |

## Device Types

| Node | Zigbee Role | Device ID | Endpoints |
|------|-------------|-----------|-----------|
| Hub | Coordinator | 0xFC00 | 1 (Control), 2 (Alert) |
| Valve Controller | Router | 0xFC01 | 1 (Control), 2 (Alert) |
| Pipe Sensor | Router | 0xFC02 | 1 (Sensor), 2 (Alert) |
| Appliance Monitor | End Device | 0xFC03 | 1 (Sensor), 2 (Alert) |

## Custom Clusters

### Cluster 0xFC00 — FlowGuard Control

**Server cluster on all nodes. Client cluster on hub.**

| Attribute ID | Data Type | Access | Description |
|-------------|-----------|--------|-------------|
| 0x0000 | enum8 | R | Valve state (0=open, 1=closed, 2=closing, 3=opening, 4=error) |
| 0x0001 | uint16 | R | Flow rate (mL/min) |
| 0x0002 | uint16 | R | Pressure (kPa × 10) |
| 0x0003 | int16 | R | Temperature (°C × 100) |
| 0x0004 | enum8 | R | Leak state (0=dry, 1=wet, 2=alert, 3=confirmed) |
| 0x0005 | uint16 | R | Vibration RMS (mg × 10) |
| 0x0006 | uint8 | R | Acoustic anomaly score (0-255) |

**Reporting Configuration:**
- Minimum interval: 10 seconds (normal), 1 second (alert)
- Maximum interval: 300 seconds (5 minutes)
- Reportable change: 10% of attribute value

### Cluster 0xFC01 — FlowGuard Command

**Server cluster on valve controller only. Client cluster on hub.**

| Command ID | Name | Payload | Description |
|-----------|------|---------|-------------|
| 0x00 | ValveOpen | auth_token[4], reason[1] | Open valve (requires 2FA) |
| 0x01 | ValveClose | auth_token[4], reason[1] | Close valve |
| 0x02 | StartAcousticCapture | duration_sec[1] | Trigger acoustic capture on pipe sensor |
| 0x03 | SetSamplingRate | sensor_mask[2], rate_hz[2] | Change sensor sampling rate |
| 0x04 | EmergencyShutdown | auth_token[4], source[1] | Emergency close all valves |
| 0x05 | ResetNode | node_id[2] | Reset a specific node |

### Cluster 0xFC02 — FlowGuard Alert

**Server cluster on hub. Client cluster on all nodes.**

| Attribute ID | Data Type | Access | Description |
|-------------|-----------|--------|-------------|
| 0x0000 | enum8 | R | Alert level (0=info, 1=warning, 2=critical, 3=emergency) |
| 0x0001 | enum8 | R | Alert type (0=leak, 1=pressure, 2=freeze, 3=hammer, 4=appliance, 5=battery, 6=flow) |
| 0x0002 | string(64) | R | Alert message |

## Standard Clusters Used

| Cluster ID | Name | Nodes |
|-----------|------|-------|
| 0x0000 | Basic | All |
| 0x0001 | Power Configuration | All (battery level reporting) |
| 0x0003 | Identify | All (pairing blink) |
| 0x0006 | On/Off | Valve controller (valve open/close abstraction) |
| 0x0402 | Temperature Measurement | Pipe sensor, appliance monitor |
| 0x0405 | Relative Humidity | Pipe sensor, appliance monitor |
| 0x0400 | Illuminance | (Reserved for LED control) |

## Binding Configuration

```
Hub (Coordinator)
  ├── Bound to Valve Controller: 0xFC00 (Control), 0xFC01 (Command)
  ├── Bound to Pipe Sensor 1..N: 0xFC00 (Control), 0xFC02 (Alert)
  └── Bound to Appliance Monitor 1..N: 0xFC00 (Control), 0xFC02 (Alert)

Valve Controller (Router)
  └── Bound to Hub: 0xFC00 (Control), 0xFC02 (Alert)

Pipe Sensors (Routers)
  └── Bound to Hub: 0xFC00 (Control), 0xFC02 (Alert)
  └── Relay traffic for other pipe sensors (mesh routing)

Appliance Monitors (End Devices)
  └── Bound to Hub: 0xFC00 (Control), 0xFC02 (Alert)
```

## Commissioning Process

1. **Mobile app** connects to hub via BLE
2. User presses "Add Device" in app
3. Hub enters Zigbee permit-joining mode (60 seconds)
4. New device is powered on, joins network with Install Code
5. Hub assigns node ID and sends configuration
6. App displays confirmation with sensor placement guidance

## OTA Firmware Updates

- Hub checks cloud for updates daily
- Updates are downloaded to SD card
- Hub pushes updates to nodes via Zigbee OTA cluster
- Updates are signed with Ed25519 and verified before installation
- Node confirms update by reporting new firmware version
- If update fails, node rolls back to previous version