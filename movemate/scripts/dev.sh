#!/bin/sh
set -eu
cd "$(dirname "$0")/../software/dashboard"
python3 -m unittest -v
