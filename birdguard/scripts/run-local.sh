#!/bin/sh
set -eu
printf '%s\n' 'BirdGuard prototype setup: install Python dependencies in a virtual environment, then run:' 'cd software/dashboard && uvicorn app:app --host 127.0.0.1' 'Keep API bound to loopback; authentication is not implemented.'
