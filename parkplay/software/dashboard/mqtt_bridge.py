"""Optional MQTT ingest loop; requires broker TLS and per-device ACLs in deployment."""
import json
import os
from paho.mqtt.client import Client
from app import Reading, ingest


def main() -> None:
    host = os.environ["MQTT_HOST"]
    topic = os.environ.get("MQTT_TOPIC", "parkplay/v1/+/+/telemetry")
    client = Client()
    client.tls_set()  # system CA; production must validate broker identity
    client.username_pw_set(os.environ["MQTT_USER"], os.environ["MQTT_PASSWORD"])
    def on_connect(c, _userdata, _flags, reason_code, _properties):
        if reason_code == 0: c.subscribe(topic, qos=1)
    def on_message(_c, _userdata, msg):
        try:
            reading = Reading.model_validate(json.loads(msg.payload))
            ingest(reading)
        except Exception:
            # Invalid telemetry is dropped; never log raw payloads or credentials.
            return
    client.on_connect, client.on_message = on_connect, on_message
    client.connect(host, int(os.environ.get("MQTT_PORT", "8883")), keepalive=30)
    client.loop_forever()

if __name__ == "__main__": main()
