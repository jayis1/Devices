"""
TremorSync MQTT Handler - async MQTT subscriber that routes messages to DB.
"""

import asyncio
import json
import logging
from typing import Callable, Optional

import aiomqtt

logger = logging.getLogger("tremorsync.mqtt")

class MQTTHandler:
    def __init__(self, broker: str, port: int = 1883,
                 on_message: Optional[Callable] = None):
        self.broker = broker
        self.port = port
        self.on_message = on_message
        self._task: Optional[asyncio.Task] = None

    async def start(self):
        self._task = asyncio.create_task(self._run())

    async def stop(self):
        if self._task:
            self._task.cancel()

    async def _run(self):
        retries = 0
        while True:
            try:
                async with aiomqtt.Client(self.broker, port=self.port) as client:
                    await client.subscribe("tremorsync/#")
                    logger.info("MQTT connected to %s:%d", self.broker, self.port)
                    retries = 0
                    async for msg in client.messages:
                        await self._handle(msg)
            except Exception as e:
                retries += 1
                delay = min(2 ** retries, 60)
                logger.warning("MQTT error: %s — retry in %ds", e, delay)
                await asyncio.sleep(delay)

    async def _handle(self, msg):
        try:
            payload = json.loads(msg.payload.decode())
        except json.JSONDecodeError:
            logger.warning("Invalid JSON on %s", msg.topic)
            return
        topic = str(msg.topic)
        logger.debug("MQTT %s: %s", topic, payload)
        if self.on_message:
            await self.on_message(topic, payload)

    async def publish(self, topic: str, data: dict):
        async with aiomqtt.Client(self.broker, port=self.port) as client:
            await client.publish(topic, json.dumps(data))