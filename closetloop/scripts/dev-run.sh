#!/bin/sh
set -eu
python3 -m pip install -r software/dashboard/requirements.txt
printf '%s\n' 'Run uvicorn main:app from software/dashboard after reviewing network exposure.'
