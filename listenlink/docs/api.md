# API operation

Install software/dashboard/requirements.txt and run `uvicorn main:app --app-dir software/dashboard --host 127.0.0.1`. Demo API is unauthenticated and must remain loopback-only. POST /v1/readings accepts validated JSON as documented in protocol.md; GET /v1/readiness reports freshness. MQTT is a deployment integration requirement and is not implemented in this prototype.
