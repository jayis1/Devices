# GreaseLoop integration guide

1. Build two low-voltage prototypes on bench supplies before enclosure assembly. Verify all grounds, 3.3V logic, and HX711 bridge wiring; use a certified external USB supply only.
2. Flash node firmware using the selected vendor SDK after implementing the documented board drivers. The current C programs are protocol examples, not complete radio firmware.
3. Configure the hub for local-only operation. Production MQTT must require TLS, per-node credentials and topic ACLs; never enable anonymous access or expose the development API publicly.
4. Calibrate the load platform with a known reference mass, record uncertainty and tare procedure. Validation is not legal-for-trade measurement.
5. Check local used-oil collection rules and perform supervised manual handoffs; no automatic collection or disposal claim is made.

No deployment script is included because hardware, broker credentials, and local collection requirements are not available in this prototype.
