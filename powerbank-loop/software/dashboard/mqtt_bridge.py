"""MQTT-to-HTTP bridge prototype. Requires broker TLS and client credentials."""
import os
import re
import requests
import paho.mqtt.client as mqtt

site_pattern = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
api = os.environ["PBL_API_URL"]
site = os.environ["PBL_SITE"]
if not site_pattern.fullmatch(site):
    raise SystemExit("invalid PBL_SITE")
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"pbl-bridge-{site}")
client.username_pw_set(os.environ["PBL_MQTT_USER"], os.environ["PBL_MQTT_PASSWORD"])
client.tls_set(ca_certs=os.environ["PBL_MQTT_CA"])

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        client.subscribe(f"powerbankloop/v1/{site}/+/telemetry", qos=1)

def on_message(client, userdata, message):
    if len(message.payload) > 4096:
        return
    response = requests.post(f"{api.rstrip('/')}/v1/events", data=message.payload,
                             headers={"content-type":"application/json"}, timeout=5,
                             verify=os.environ["PBL_API_CA"])
    response.raise_for_status()

client.on_connect = on_connect
client.on_message = on_message
client.connect(os.environ["PBL_MQTT_HOST"], int(os.getenv("PBL_MQTT_PORT", "8883")), 60)
client.loop_forever()
