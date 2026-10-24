# LibraryLoop deploy notes

Manual development only. Install Python requirements in an isolated environment, then run `uvicorn app:app --host 127.0.0.1 --port 8000` from this directory. Do not bind publicly: authentication and TLS are not implemented. MQTT connection is documented only; no broker client is implemented.
