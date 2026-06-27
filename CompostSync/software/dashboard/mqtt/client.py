"""
CompostSync Backend — MQTT Client
Receives telemetry from hubs, publishes commands and alerts.
"""
import asyncio
import json
import logging
from datetime import datetime

import paho.mqtt.client as mqtt

from config import settings
from ml.inference import ml_inference

logger = logging.getLogger("compostsync.mqtt")


class MQTTClient:
    def __init__(self):
        self.client = mqtt.Client(client_id="compostsync-backend")
        self.connected = False
        self._setup()

    def _setup(self):
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        if settings.mqtt_user:
            self.client.username_pw_set(settings.mqtt_user, settings.mqtt_pass)

    @property
    def is_connected(self):
        return self.connected

    async def connect(self):
        """Connect to MQTT broker in a background thread."""
        loop = asyncio.get_event_loop()
        await loop.run_in_executor(None, self._connect_blocking)

    def _connect_blocking(self):
        try:
            self.client.connect(settings.mqtt_host, settings.mqtt_port, 60)
            self.client.loop_start()
        except Exception as e:
            logger.error(f"MQTT connect failed: {e}")

    async def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()

    def _on_connect(self, client, userdata, flags, rc):
        self.connected = True
        logger.info(f"MQTT connected (rc={rc})")
        self.client.subscribe("compostsync/+/+/telemetry", qos=1)
        self.client.subscribe("compostsync/+/+/status", qos=1)

    def _on_disconnect(self, client, userdata, rc):
        self.connected = False
        logger.warning(f"MQTT disconnected (rc={rc})")

    def _on_message(self, client, userdata, msg):
        """Handle incoming MQTT messages."""
        try:
            topic_parts = msg.topic.split("/")
            user_id = topic_parts[1]
            node_id = topic_parts[2]
            msg_type = topic_parts[3]

            payload = json.loads(msg.payload)
            logger.info(f"MQTT [{msg_type}] from {node_id}: {payload}")

            if msg_type == "telemetry":
                asyncio.create_task(self._handle_telemetry(user_id, node_id, payload))
            elif msg_type == "status":
                logger.info(f"Status update from {node_id}: {payload}")

        except Exception as e:
            logger.error(f"MQTT message error: {e}")

    async def _handle_telemetry(self, user_id, node_id, data):
        """Store telemetry and trigger ML inference."""
        # Store in database (async)
        # await db_store_telemetry(user_id, node_id, data)

        # Check for critical alerts
        alerts_flags = data.get("alerts", 0)
        if alerts_flags & 0x01:  # ALERT_METHANE_HIGH
            await self._send_alert(user_id, node_id, {
                "type": "methane_high",
                "severity": 2,
                "message": f"Methane level {data.get('ch4', 0)} ppm — TURN PILE NOW",
                "action": "turn_pile",
            })

        # Trigger ML inference
        result = await ml_inference.predict(data)
        if result:
            # Publish ML results
            topic = f"compostsync/{user_id}/ml/forecast"
            self.client.publish(topic, json.dumps(result), qos=1)

    async def _send_alert(self, user_id, node_id, alert_data):
        """Send alert via MQTT and push notification."""
        topic = f"compostsync/{user_id}/alerts"
        self.client.publish(topic, json.dumps(alert_data), qos=2)
        logger.warning(f"Alert sent: {alert_data}")

    def publish_command(self, user_id, node_id, command):
        """Send a command to a specific node."""
        topic = f"compostsync/{user_id}/{node_id}/command"
        self.client.publish(topic, json.dumps(command), qos=1)


# Global instance
mqtt_client = MQTTClient()