#!/usr/bin/env python3
"""Print local run instructions; intentionally makes no system changes."""
print("Install software/dashboard/requirements.txt in an isolated environment, then run:")
print("  uvicorn main:app --host 127.0.0.1 --port 8000")
print("API is local-only by default. Configure a broker adapter and authentication before network exposure.")
