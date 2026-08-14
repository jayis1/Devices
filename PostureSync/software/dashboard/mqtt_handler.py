"""
PostureSync MQTT Handler
Async MQTT client for receiving sensor data from Hub
"""

import asyncio
import json
import logging
import ssl
from typing import Callable, Optional
import aiomqtt

logger = logging.getLogger("postsync.mqtt")


class PostureSyncMQTTHandler:
    """Handles MQTT communication with PostureSync Hub"""

    def __init__(self, broker: str = "broker.postsync.io", port: int = 8883):
        self.broker = broker
        self.port = port
        self.client: Optional[aiomqtt.Client] = None
        self.handlers = {}
        self.running = False

    def register_handler(self, topic: str, handler: Callable):
        """Register a handler for a specific MQTT topic"""
        self.handlers[topic] = handler

    async def connect(self):
        """Connect to MQTT broker with TLS"""
        tls_params = aiomqtt.TLSParameters(
            ca_certs=None,
            certfile=None,
            keyfile=None,
            cert_reqs=ssl.CERT_REQUIRED,
            tls_version=ssl.PROTOCOL_TLS_CLIENT,
        )

        self.client = aiomqtt.Client(
            hostname=self.broker,
            port=self.port,
            tls_params=tls_params,
            username="postsync_hub",
            password="",
        )

        await self.client.__aenter__()
        logger.info(f"Connected to MQTT broker at {self.broker}:{self.port}")

    async def subscribe(self, topic: str):
        """Subscribe to a topic"""
        if self.client:
            await self.client.subscribe(topic)
            logger.info(f"Subscribed to {topic}")

    async def publish(self, topic: str, payload: dict, qos: int = 1):
        """Publish a message"""
        if self.client:
            await self.client.publish(topic, json.dumps(payload), qos=qos)

    async def listen(self):
        """Listen for incoming MQTT messages"""
        self.running = True
        try:
            async with self.client as client:
                await client.subscribe("postsync/#")
                async for message in client.messages:
                    topic = str(message.topic)
                    payload = json.loads(message.payload.decode())
                    logger.debug(f"MQTT RX: {topic} = {payload}")

                    # Route to registered handlers
                    for pattern, handler in self.handlers.items():
                        if pattern in topic or topic in pattern:
                            try:
                                await handler(payload)
                            except Exception as e:
                                logger.error(f"Handler error for {topic}: {e}")

        except Exception as e:
            logger.error(f"MQTT listen error: {e}")
        finally:
            self.running = False

    async def disconnect(self):
        """Disconnect from MQTT broker"""
        if self.client:
            await self.client.__aexit__(None, None, None)
            logger.info("MQTT disconnected")


# Topic structure:
# postsync/sensor/spine_band   — Spine Band IMU + PPG data
# postsync/sensor/posture_garment — Posture Garment EMG + IMU data
# postsync/sensor/chair_pad    — Chair Pad pressure data
# postsync/sensor/desk_sentinel — Desk Sentinel distance + light data
# postsync/alerts              — Posture alerts
# postsync/commands/{node_id} — Commands to nodes