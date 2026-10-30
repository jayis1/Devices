# Protocol v1

BLE carries a UTF-8 JSON object capped at 160 bytes with required fields `v` (1), `node` (site-unique string), `seq` (uint32), `kind`, and `value`. Supported kinds: `passage` (integer count), `flap` (boolean), `bin_mass_g` (nonnegative numeric grams), and `heartbeat` (battery millivolts in value in a future revision; currently reserved). Gateways reject unsupported versions/kinds and oversize messages, deduplicate by node+sequence and flag sequence gaps. This prototype has no authenticated BLE frame or durable counter; do not deploy publicly as a secure device.
