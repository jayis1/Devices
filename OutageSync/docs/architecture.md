# OutageSync Architecture

## Design goals

1. Preserve human safety first.
2. Protect food, medicine, and communications second.
3. Minimize generator runtime, noise, and fuel burn.
4. Fail gracefully under internet loss.
5. Provide explainable recommendations during stressful events.

## Data flow

1. Panel controller detects grid quality degradation and branch state.
2. Hub fuses this with cold-chain, outlet, and generator telemetry.
3. ML inference computes outage duration, load priorities, and comfort/cold-chain risks.
4. Hub publishes commands to outlet nodes and panel controller.
5. Mobile app and dashboard show plain-language guidance.
6. When service returns, staged restoration avoids inrush spikes.

## Edge vs cloud split

- **Edge required:** outage detection, load shedding, fuel safety interlocks, cold hold-time alarms
- **Cloud optional:** fleet analytics, utility correlation, backups, caregiver sharing
- **Cellular fallback:** emergency notifications and remote status when broadband is down

## Power domains

- SELV sensor nodes: 3V0 to 3V3
- Hub digital: 5V + 3V3
- Fuel sentinel: 12V input + 5V + 3V3
- Panel controller: isolated 24V -> 12V/5V/3V3 with HV sensing front-end
- Outlet node: mains AC -> isolated 5V/3V3 with latching relay pulse rail

## Reliability strategy

- hub watchdog on RP2040
- latching relays retain safe state
- panel node can locally inhibit unsafe generator start
- protocol includes sequence numbers and CRC
- local SQLite keeps working offline
