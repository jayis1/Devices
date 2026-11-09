"""Optional MQTT-to-API event adapter. Requires paho-mqtt; TLS setup is deployment-owned."""
import json
import os
from fastapi import HTTPException
from main import Event, ingest


def start():
    import paho.mqtt.client as mqtt
    host=os.environ['MENDMATE_MQTT_HOST']
    client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='mendmate-hub')
    username=os.environ.get('MENDMATE_MQTT_USERNAME')
    password=os.environ.get('MENDMATE_MQTT_PASSWORD')
    if username: client.username_pw_set(username,password)
    # For production, configure CA validation before connect; never disable TLS verification.
    client.tls_set(ca_certs=os.environ['MENDMATE_MQTT_CA'])
    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0: client.subscribe('mendmate/v1/+/telemetry', qos=1)
    def on_message(client, userdata, message):
        if len(message.payload)>1024: return
        try:
            data=json.loads(message.payload)
            event=Event.model_validate(data)
            ingest(event)
        except (ValueError, TypeError, HTTPException):
            return
    client.on_connect=on_connect; client.on_message=on_message
    client.connect(host, int(os.environ.get('MENDMATE_MQTT_PORT','8883')), keepalive=30)
    client.loop_start()
    return client
