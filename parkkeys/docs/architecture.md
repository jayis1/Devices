# ParkKeys architecture

Instrument station and weather node send observation events to a local site hub over Wi-Fi MQTT. The hub is the availability boundary: visitors can view the last known status locally without internet, while remote access is out of scope for this prototype. The instrument button records an explicit human report; cover reed state is context only. Weather data is observational and does not decide whether the piano is usable. Stewards inspect and decide next steps.

Failure behavior: the station must never display a successful submission until an acknowledgement is received. Stale reports show their age. Sensor disconnect is represented as missing data, never as a nominal reading. Hub restart loses the current in-memory reference state; production must add durable storage before deployment. Nothing actuates the piano or cover. Use only isolated, low-voltage USB power and weather-protected enclosures installed by a qualified steward.
