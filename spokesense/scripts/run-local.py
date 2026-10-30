#!/usr/bin/env python3
"""Local demo launcher; binds only to loopback and requires uvicorn installed."""
import subprocess, sys
raise SystemExit(subprocess.call([sys.executable, '-m', 'uvicorn', 'app:app', '--app-dir', 'software/dashboard', '--host', '127.0.0.1', '--port', '8000']))
