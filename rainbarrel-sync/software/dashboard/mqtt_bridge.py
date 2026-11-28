"""Optional MQTT publisher; requires an operator-configured TLS broker and credentials."""
import json
import os
import ssl
import paho.mqtt.client as mqtt

def publish_event(event: dict) -> None:
    host = os.environ["MQTT_HOST"]
    site = os.environ["MQTT_SITE"]
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"rainbarrel-{site}")
    client.username_pw_set(os.environ["MQTT_USERNAME"], os.environ["MQTT_PASSWORD"])
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    client.connect(host, int(os.getenv("MQTT_PORT", "8883")), keepalive=30)
    topic = f"rainbarrel/v1/{site}/{event['node_id']}/telemetry"
    client.publish(topic, json.dumps(event, default=str), qos=1, retain=False).wait_for_publish(timeout=5)
    client.disconnect()
