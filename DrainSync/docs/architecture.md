# DrainSync Architecture

## 1. Topology

DrainSync models the home as a directed plumbing graph:

- fixture branch nodes: sinks, showers, tubs, standpipes
- trap protection nodes: floor drains and rarely used receptors
- trunk/stack node: main stack monitor
- actuation node: backwater valve / cleanout actuator
- coordination node: hub gateway

Each telemetry record is tagged with:

- `node_id`
- `zone`
- `fixture_type`
- `branch_id`
- `event_class`
- `risk_score`
- `ts`

## 2. Edge/Cloud split

### Edge on hub
- radio scheduling
- short-term buffering
- command arbitration
- local valve safety policies
- offline notifications on LAN
- cached inference for critical clog / backup decisions

### Cloud/backend
- longitudinal analytics
- artifact versioning
- fleet telemetry ingest
- app sync
- multi-home installer dashboards

## 3. Decision ladder

1. Node computes local features.
2. Node transmits compressed summary frame.
3. Hub enriches with topology, time, weather, and recent events.
4. Risk models score clog, odor, and backup threats.
5. Policy engine chooses observe / notify / recommend / actuate.
6. Critical actions require a confidence threshold and source diversity.

## 4. Critical automation rules

### Trap priming
Allowed only when:
- trap depth below threshold
- no active leak on floor ring
- upstream water available
- cycle budget remaining

### Backwater closure
Allowed when any of the following is true:
- reverse pressure sustained > configured threshold and main level rising
- municipal storm state red and stack surge count high
- explicit service mode request

### Backwater re-open
Allowed when:
- pressure normalized
- cleanout level falling
- manual acknowledgement or policy timer elapsed

## 5. Data retention

- raw waveform snippets: 24 h rolling
- minute summaries: 90 d
- event records: 2 y
- valve actuation log: indefinite

## 6. Serviceability

- per-node calibration profiles are exportable as JSON
- service mode exposes live sensor page and motor diagnostics
- all critical nodes have local LEDs and hardware override paths
