# RepairLoop — community repair-event intake and bench coordination

RepairLoop helps volunteer-run repair cafés keep an orderly intake queue and direct each item to a currently available repair bench. Visitors receive a short, random ticket; volunteers record only a coarse item category and repair outcome. It is an event operations aid, not a repair diagnosis, safety certification, or guarantee that an item can be repaired.

## System contract and boundaries

A small event gateway coordinates two or more bench beacons and an optional ticket dock. Bench state is explicitly set by a volunteer button (available / busy / paused); it is not inferred from people or cameras. A ticket has a random event-scoped ID, category, arrival time, and optional volunteer-entered status. The gateway assigns the next eligible ticket by category compatibility and oldest arrival; a human can override. It runs on the venue LAN with SQLite and remains usable without internet. No names, contact details, audio, images, or visitor tracking are collected. Delete event data at close; export is an operator action.

Design targets (not measured): local ticket lookup under 1 second; no lost acknowledged ticket on process restart; all queue records exportable as CSV. Hardware is low-voltage USB-powered prototype equipment. No actuator, mains connection, medical function, or life-safety role.

## Nodes

| Node | Reference parts | Responsibilities | Link |
|---|---|---|---|
| Event gateway | Raspberry Pi Zero 2 W, microSD, 5 V USB supply | Local API, SQLite queue, MQTT broker/client, assignments, operator display | Wi-Fi Ethernet bridge / LAN |
| Bench beacon ×2+ | ESP32-C3-MINI-1, two buttons, RGB LED, USB 5 V supply with 3.3 V regulator | Volunteer-declared state, ticket call acknowledgment, visible state | BLE advertisements to gateway; USB serial for bring-up |
| Ticket dock (optional) | ESP32-C3-MINI-1, 128×64 SSD1306 OLED, three buttons | Create anonymous event ticket, display ticket code | BLE to gateway; dock can be replaced by web tablet |

The ESP32-C3 module's GPIO assignment and power design are prototype selections; verify the exact module reference design, strapping pins, decoupling, antenna keep-out and regulator transient behavior before PCB layout. LEDs are status-only. A beacon losing connectivity must show an unmistakable offline indication and cannot claim a ticket was acknowledged.

## Pin / interface ledger

Bench beacon (ESP32-C3-MINI-1): GPIO4 available button (active-low, pull-up), GPIO5 busy button, GPIO6 RGB data through 330 Ω, GPIO20/21 UART0 service, 3V3/GND supply. Confirm selected development board routing and boot strapping before assembly; pinout is not a certified PCB netlist. Use a current-limited USB 5 V supply and a regulator/module rated for radio peaks. Keep outputs inactive at reset.

Ticket dock: GPIO4/5/6 buttons; I²C SDA/SCL GPIO8/9 to SSD1306 at 3.3 V, 4.7 kΩ pull-ups; UART0 GPIO20/21. Verify address and board pin mapping on the selected module. No battery charging circuit is specified.

## Data flow and protocol

1. Volunteer starts an event locally; gateway creates an unpredictable event UUID.
2. A dock or tablet requests a ticket with an allowlisted category. Gateway persists it before returning the ticket number.
3. Bench beacons advertise protocol version, paired bench identifier, sequence, and coarse state. Pairing is a deliberate local operator action.
4. Gateway selects a compatible waiting ticket, publishes a bounded call, and updates state only after a volunteer acknowledgment. Duplicate calls are idempotent by event ID and ticket ID.
5. Web dashboard polls the local API and shows queue age, bench state, and stale/offline warnings.

MQTT topics on the private local broker: `repairloop/v1/<event>/bench/<id>/state` and `repairloop/v1/<event>/ticket/<id>/call`. QoS 1; consumers deduplicate by event-scoped message ID. LAN-only deployment, anonymous broker access disabled, unique credentials and TLS required if traffic leaves the venue. Radio advertisements are not authenticated and are not suitable for hostile venues; production needs authenticated pairing and message integrity using a reviewed standard, not custom cryptography. Queue API does not expose arbitrary remote commands. No cloud dependency or cloud credentials are included.

Offline behavior: gateway keeps a bounded persistent SQLite queue; beacon reconnect uses exponential backoff with jitter and reports last sequence; stale state is shown as unknown, never available. Ticket creation reports failure unless locally committed. Data retention defaults to event close plus operator confirmation; no background sync. Queue overflow refuses new ticket creation with an explicit error instead of evicting acknowledged tickets.

## Power and safety

Gateway: certified external 5 V USB supply sized for the Pi and display; use quality microSD and clean shutdown. Beacons/dock: certified 5 V USB supplies; module regulator must handle radio bursts. No claimed battery life. Brownout recovery is reboot to idle/offline state; no persistent actuation. Operator must provide a physical queue fallback during power/network loss. This prototype is not a public-use certified product.

## Repository map and readiness

- `schematic/`: connection ledgers; these are not KiCad schematics and have no ERC evidence.
- `firmware/`: portable C state/protocol examples, not target-board applications.
- `hardware/bom/`: prototype parts and sourcing caveats.
- `software/dashboard/`: local FastAPI/SQLite queue API and unit tests.
- `software/ml-pipeline/`: optional synthetic wait-time baseline; no real dataset or accuracy claim.
- `software/mobile-app/`: React Native operator display stub.
- `docs/`: architecture, protocol, and API details.
- `scripts/`: development helper.

## Run and verify

`cd software/dashboard && python3 -m unittest -v`

`cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/repairloop-protocol && /tmp/repairloop-protocol`

`python3 software/ml-pipeline/train.py`

Host-side tests do not verify radio, broker integration, electrical design, RF compliance, physical reliability, deployment security, or mobile compilation. No KiCad ERC, target firmware build, physical prototype test, or ML evaluation has been performed. Treat this as a reference prototype, not a fabrication-ready or safety-certified system.

Invented and maintained by [jayis1](https://github.com/jayis1).
