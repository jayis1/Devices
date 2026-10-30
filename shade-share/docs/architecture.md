# Architecture

ShadeShare observes public shade assets, not people. Two battery node types report discrete shade/tamper events and surface temperature to a mains-free RP2040/LoRa gateway. The gateway timestamps, validates, deduplicates, persists locally, and optionally forwards records over TLS to the operator backend. During WAN failure it queues records; stale status is explicitly unknown. Human stewards confirm any maintenance action.

## Interfaces and failure policy

Gateway is coordinator and store-and-forward boundary. Node identity is provisioned at installation; the current prototype does not implement secure provisioning and must not be used on an untrusted radio network. Sensor open/short, low battery, CRC failure or missing heartbeat yield unknown/maintenance-needed status. Sensor nodes are read-only with no safety-critical control path. Local log rotation is bounded to 30 days. Internet loss leaves radio collection/local export available; gateway loss disables fresh status and directory availability must be marked stale.

## Electrical assumptions

3.3 V logic throughout. Gateway has regulated USB 5 V input; node supplies are 2×AA primary lithium with a low-quiescent 3.3 V regulator sized for radio peak current. Fit 100 nF at each IC supply and 10 µF at radio supply. Radio antenna placement and matching follow module vendor guidance; final RF layout and EMC/regulatory review are outstanding. No PCB or validated KiCad source is included.

## Threat and safety boundary

Read-only telemetry; no physical actuator. Assume radio is observable and replayable until authenticated encryption is implemented. Do not send personally identifying data. Cloud transport requires TLS and authenticated broker credentials supplied out of band. This prototype is advisory and cannot certify shade, surface safety, or accessibility.
