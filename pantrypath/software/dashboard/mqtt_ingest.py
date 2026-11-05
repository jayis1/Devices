"""MQTT topic allow-list helper; TLS/auth are configured by deployment, not here."""
def allowed_topic(topic: str) -> bool:
    parts=topic.split("/")
    return len(topic)<=128 and len(parts)==3 and parts[0]=="pantrypath" and parts[1]=="v1" and parts[2].isdigit()
