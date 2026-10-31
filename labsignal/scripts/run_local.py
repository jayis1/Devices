#!/usr/bin/env python3
"""Local prototype launcher hint; deployment security is not provided."""
import os
print("Set LABSIGNAL_BIND=127.0.0.1 for local-only use; add TLS/auth before network exposure.")
os.execvp("uvicorn", ["uvicorn", "app:app", "--host", os.getenv("LABSIGNAL_BIND", "127.0.0.1"), "--port", "8080"])
