"""Optional MQTT subscriber bridge; caller supplies broker auth/TLS configuration."""
import json
import os
import paho.mqtt.client as mqtt
from app import Telemetry, ingest


def run() -> None:
    host = os.environ['MQTT_HOST']
    port = int(os.getenv('MQTT_PORT', '8883'))
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(os.environ['MQTT_USERNAME'], os.environ['MQTT_PASSWORD'])
    client.tls_set()
    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            client.subscribe('snowshare/v1/+/+/telemetry', qos=1)
    def on_message(client, userdata, msg):
        try:
            item = Telemetry.model_validate(json.loads(msg.payload))
            ingest(item)
        except Exception:
            # Invalid messages are discarded; production must emit a redacted metric.
            return
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(host, port, keepalive=30)
    client.loop_forever()

if __name__ == '__main__':
    run()
