import os
import paho.mqtt.client as mqtt
from app import Telemetry, latest

TOPIC = "quietpath/v1/telemetry/+"

def on_message(_client, _userdata, message):
    try:
        report = Telemetry.model_validate_json(message.payload)
        item = report.model_dump()
        latest[report.node_id] = item
    except Exception:
        # Invalid MQTT payload is dropped; production should emit a redacted metric.
        return

def run():
    host = os.environ["MQTT_HOST"]
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(os.environ["MQTT_USER"], os.environ["MQTT_PASSWORD"])
    client.tls_set(ca_certs=os.environ["MQTT_CA"])
    client.on_message = on_message
    client.connect(host, int(os.getenv("MQTT_PORT", "8883")))
    client.subscribe(TOPIC, qos=1)
    client.loop_forever()

if __name__ == "__main__":
    run()
