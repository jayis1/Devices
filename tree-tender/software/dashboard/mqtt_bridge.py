"""Optional TLS MQTT-to-API bridge; run separately from the API process."""
import json, os, ssl, urllib.request
import paho.mqtt.client as mqtt

HOST=os.environ['MQTT_HOST']
USER=os.environ['MQTT_USERNAME']
PASSWORD=os.environ['MQTT_PASSWORD']
API=os.getenv('API_BASE_URL','http://127.0.0.1:8000')
SITE=os.environ['TREE_TENDER_SITE']

def on_message(client, userdata, message):
    try:
        payload=json.loads(message.payload)
        request=urllib.request.Request(f"{API}/sites/{SITE}/observations", data=json.dumps(payload).encode(), headers={'Content-Type':'application/json'}, method='POST')
        with urllib.request.urlopen(request, timeout=5) as response:
            if response.status not in (200,201): raise RuntimeError(f'API status {response.status}')
    except Exception as exc:
        # No payload or credential logging.
        print(f'Observation forwarding failed: {type(exc).__name__}')

client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='treetender-bridge')
client.username_pw_set(USER,PASSWORD)
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
client.on_message=on_message
client.connect(HOST, int(os.getenv('MQTT_PORT','8883')), 60)
client.subscribe(f'treetender/v1/{SITE}/+/observation', qos=1)
client.loop_forever()
