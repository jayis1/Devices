#!/bin/sh
set -eu
printf '%s\n' 'BatteryLoop prototype only. No deployment is performed.' 'Run the API locally on loopback: uvicorn app:app --host 127.0.0.1 --port 8000' 'Do not expose this unauthenticated API to a network.'
