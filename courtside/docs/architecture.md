# Architecture

Three distinct roles: fixed gateway coordinates the site and WAN sync; surface sentinel samples sensor inputs; inspection beacon lets staff record explicit human observations. BLE GATT is a star link, with gateway as central. No node actuates court equipment. Gateway time is UTC; leaves attach local sample time when available. During hub loss leaves keep sampling locally only if storage is fitted; reference firmware does not yet implement persistent buffering. During internet loss gateway should buffer as described in protocol, but durable queue is not implemented in demo. Hardware/environmental validation is outstanding.

Threat model: public physical access, radio spoofing, lost gateway, clock drift, water ingress, sensor drift. Demo is not secure for public deployment. Production needs device enrollment, BLE link encryption, HTTPS/MQTT TLS, authentication/authorization, bounded queue, signed OTA and local access controls. Firmware update support is not implemented.
