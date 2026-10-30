#!/bin/sh
set -eu
python3 -m compileall -q software/dashboard software/ml-pipeline
printf '%s\n' 'Python syntax checks passed.'
printf '%s\n' 'Hardware deployment is intentionally not performed by this script.'
