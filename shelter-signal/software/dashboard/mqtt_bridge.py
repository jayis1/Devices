"""Topic helper only; TLS client and site-scoped ACL are deployment requirements."""
def telemetry_topic(site_id: str) -> str:
    if not site_id or any(ch in site_id for ch in "/+#"):
        raise ValueError("invalid site id")
    return f"shelter-signal/{site_id}/telemetry"
