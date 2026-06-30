"""
AsthmaSync — MQTT Client Module
Separate MQTT subscriber that can run as a standalone service.
"""

import asyncio
import json
import logging
import aiomqtt

logger = logging.getLogger("asthmasync.mqtt")

BROKER = "broker.asthmasync.io"
PORT = 1883
TOPICS = ["asthmasync/telemetry", "asthmasync/events"]


class AsthmaMQTTClient:
    """Async MQTT client for receiving hub telemetry."""

    def __init__(self, broker=BROKER, port=PORT, on_message=None):
        self.broker = broker
        self.port = port
        self.on_message = on_message
        self._running = False

    async def start(self):
        self._running = True
        while self._running:
            try:
                async with aiomqtt.Client(self.broker, port=self.port) as client:
                    for topic in TOPICS:
                        await client.subscribe(topic)
                    logger.info(f"MQTT connected to {self.broker}, subscribed to {TOPICS}")

                    async for message in client.messages:
                        try:
                            payload = json.loads(message.payload.decode())
                            if self.on_message:
                                await self.on_message(payload)
                        except Exception as e:
                            logger.error(f"Message processing error: {e}")
            except Exception as e:
                logger.error(f"MQTT connection error: {e}")
                await asyncio.sleep(5)

    def stop(self):
        self._running = False


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    async def handler(msg):
        print(f"Received: {msg}")

    client = AsthmaMQTTClient(on_message=handler)
    asyncio.run(client.start())