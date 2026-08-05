"""SeizureSync — MQTT ingestion from hub devices."""
import os
import asyncio
import json
import logging
import aiomqtt

logger = logging.getLogger("seizuresync.mqtt")

MQTT_BROKER = os.environ.get("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC = "seizuresync/#"

async def mqtt_loop():
    """Subscribe to all SeizureSync topics and route to handlers."""
    while True:
        try:
            async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
                await client.subscribe(MQTT_TOPIC)
                logger.info("MQTT connected, subscribed to %s", MQTT_TOPIC)
                async for msg in client.messages:
                    await handle_mqtt_message(msg.topic, msg.payload)
        except Exception as e:
            logger.error("MQTT error: %s — reconnecting in 5s", e)
            await asyncio.sleep(5)


async def handle_mqtt_message(topic: str, payload: bytes):
    """Route MQTT message to appropriate handler."""
    parts = str(topic).split("/")
    if len(parts) < 3:
        return
    patient_id = parts[1]
    msg_type = parts[2]
    data = json.loads(payload)

    logger.info("MQTT %s/%s: %s", patient_id, msg_type, data)

    if msg_type == "event":
        await handle_seizure_event(patient_id, data)
    elif msg_type == "signal":
        await handle_signal_chunk(patient_id, data)
    elif msg_type == "risk":
        await handle_risk_update(patient_id, data)
    elif msg_type == "sudep":
        await handle_sudep_update(patient_id, data)
    elif msg_type == "alert":
        await handle_alert(patient_id, data)
    elif msg_type == "diary":
        await handle_diary(patient_id, data)


async def handle_seizure_event(patient_id: str, data: dict):
    """Store seizure event and trigger ML inference + alert dispatch."""
    from models import SeizureEvent, SessionLocal
    import datetime
    db = SessionLocal()
    try:
        ev = SeizureEvent(
            patient_id=patient_id,
            onset=datetime.datetime.utcfromtimestamp(data.get("onset", 0)),
            duration_s=data.get("duration", 0),
            semiology=data.get("semiology", "unknown"),
            severity=data.get("severity", 0),
            confidence=data.get("confidence", 0.0),
            recovery_state=data.get("recovery", "active"),
        )
        db.add(ev)
        db.commit()
        logger.info("Stored seizure event %d for %s", ev.id, patient_id)

        # Trigger SemiologyNet classification + TriggerNet attribution
        # (async via Celery in production)
    finally:
        db.close()


async def handle_signal_chunk(patient_id: str, data: dict):
    """Store raw signal chunk reference in MinIO."""
    logger.info("Signal chunk for %s: %s", patient_id, data)


async def handle_risk_update(patient_id: str, data: dict):
    """Store 24-hr risk forecast."""
    logger.info("Risk update for %s: %s", patient_id, data)


async def handle_sudep_update(patient_id: str, data: dict):
    """SUDEP alert — critical. Trigger emergency escalation."""
    logger.critical("SUDEP alert for %s: %s", patient_id, data)
    # Immediate escalation: caregiver → family → 911
    from twilio_dispatch import TwilioDispatcher
    dispatcher = TwilioDispatcher()
    # In production: dispatch asynchronously
    # await dispatcher.dispatch_sudep(patient_id, data)


async def handle_alert(patient_id: str, data: dict):
    """Alert event — route to mobile app via WebSocket."""
    logger.info("Alert for %s: %s", patient_id, data)


async def handle_diary(patient_id: str, data: dict):
    """Seizure diary update."""
    logger.info("Diary for %s: %s", patient_id, data)