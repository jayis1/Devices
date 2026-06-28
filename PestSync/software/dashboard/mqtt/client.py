"""
PestSync Backend — MQTT Client
software/dashboard/mqtt/client.py
"""
import asyncio
import json
import logging
import aiomqtt

from config import settings

logger = logging.getLogger("pestsync.mqtt")


class MQTTClient:
    def __init__(self):
        self.client = None
        self.connected = False
        self._task = None

    async def connect(self):
        self._task = asyncio.create_task(self._run())
        logger.info("MQTT client task started")

    async def disconnect(self):
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
        self.connected = False
        logger.info("MQTT client disconnected")

    async def _run(self):
        while True:
            try:
                async with aiomqtt.Client(
                    hostname=settings.mqtt_host,
                    port=settings.mqtt_port,
                    username=settings.mqtt_user or None,
                    password=settings.mqtt_pass or None,
                ) as client:
                    self.client = client
                    self.connected = True
                    logger.info("MQTT connected to %s:%d", settings.mqtt_host, settings.mqtt_port)

                    # Subscribe to telemetry topics
                    await client.subscribe("pestsync/+/+/telemetry")
                    await client.subscribe("pestsync/+/+/detection")
                    await client.subscribe("pestsync/+/+/trap")
                    await client.subscribe("pestsync/+/+/deterrent")

                    async for message in client.messages:
                        await self._handle_message(message)

            except aiomqtt.MqttError as e:
                self.connected = False
                logger.warning("MQTT error: %s — reconnecting in 5s", e)
                await asyncio.sleep(5)
            except asyncio.CancelledError:
                break

    async def _handle_message(self, message):
        topic_parts = str(message.topic).split("/")
        if len(topic_parts) < 4:
            return

        user_id = topic_parts[1]
        node_id = topic_parts[2]
        msg_type = topic_parts[3]

        try:
            payload = json.loads(message.payload.decode())
        except (json.JSONDecodeError, UnicodeDecodeError):
            return

        logger.debug("MQTT %s: user=%s node=%s type=%s", msg_type, user_id, node_id, payload)

        # Route to appropriate handler
        if msg_type == "detection":
            await self._on_detection(user_id, node_id, payload)
        elif msg_type == "trap":
            await self._on_trap_event(user_id, node_id, payload)
        elif msg_type == "deterrent":
            await self._on_deterrent_status(user_id, node_id, payload)
        elif msg_type == "telemetry":
            await self._on_telemetry(user_id, node_id, payload)

    async def _on_detection(self, user_id, node_id, data):
        """Handle pest detection from sentinel."""
        pest_class = data.get("pest", "Unknown")
        confidence = data.get("conf", 0)
        logger.info("🐛 Pest detected: %s (%d%%) — user=%s node=%s",
                     pest_class, confidence, user_id, node_id)

        # Store in DB, trigger ML inference, send push notification
        # (implemented in routers/detections.py and ml/inference.py)

    async def _on_trap_event(self, user_id, node_id, data):
        """Handle trap event from smart trap."""
        status = data.get("status", "unknown")
        weight = data.get("weight", 0)
        logger.info("🎯 Trap event: %s (%dg) — user=%s node=%s",
                     status, weight, user_id, node_id)

    async def _on_deterrent_status(self, user_id, node_id, data):
        """Handle deterrent status update."""
        us_active = data.get("us", 0)
        oil = data.get("oil", 100)
        logger.debug("Deterrent status: us=%d oil=%d%% — user=%s node=%s",
                      us_active, oil, user_id, node_id)

    async def _on_telemetry(self, user_id, node_id, data):
        """Handle general telemetry."""
        logger.debug("Telemetry from %s/%s: %s", user_id, node_id, data)

    async def publish(self, topic: str, payload: dict, qos: int = 1):
        if self.client and self.connected:
            await self.client.publish(topic, json.dumps(payload), qos=qos)


mqtt_client = MQTTClient()