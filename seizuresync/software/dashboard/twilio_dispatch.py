"""SeizureSync — Twilio emergency dispatch integration."""
import os
import logging
from twilio.rest import Client

logger = logging.getLogger("seizuresync.twilio")

TWILIO_SID = os.environ.get("TWILIO_SID", "")
TWILIO_TOKEN = os.environ.get("TWILIO_TOKEN", "")
TWILIO_FROM = os.environ.get("TWILIO_FROM", "")    # Twilio phone number
EMERGENCY_NUMBER = os.environ.get("EMERGENCY_NUMBER", "911")


class TwilioDispatcher:
    """Emergency dispatch via Twilio Programmable Voice."""

    def __init__(self):
        self.client = None
        if TWILIO_SID and TWILIO_TOKEN:
            self.client = Client(TWILIO_SID, TWILIO_TOKEN)
        else:
            logger.warning("Twilio credentials not set — dispatch disabled")

    def dispatch_seizure(self, patient_name: str, patient_addr: str,
                          seizure_type: str, onset_time: str,
                          caregiver_phone: str = None):
        """Call emergency services with seizure info."""
        if not self.client:
            logger.error("Twilio not configured — cannot dispatch")
            return None

        message = (
            f"SeizureSync emergency alert. Patient {patient_name} at "
            f"{patient_addr} is experiencing a {seizure_type} seizure "
            f"that started at {onset_time}. The patient has epilepsy. "
            f"This is an automated medical alert from SeizureSync."
        )

        # Call emergency number
        call = self.client.calls.create(
            to=EMERGENCY_NUMBER,
            from_=TWILIO_FROM,
            twiml=f'<Response><Say>{message}</Say></Response>',
        )
        logger.critical("Dispatched 911 call: %s", call.sid)

        # Also notify caregiver if provided
        if caregiver_phone:
            self.client.calls.create(
                to=caregiver_phone,
                from_=TWILIO_FROM,
                twiml=f'<Response><Say>SeizureSync alert: {patient_name} '
                      f'is having a seizure. Please respond immediately.'
                      f'</Say></Response>',
            )
        return call.sid

    def dispatch_sudep(self, patient_id: str, data: dict):
        """SUDEP emergency — immediate 911 + caregiver."""
        logger.critical("SUDEP dispatch for %s", patient_id)
        # ... build message from apnea/spo2/prone data ...