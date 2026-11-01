#!/usr/bin/env sh
set -eu
: "${IDF_PATH:?Install ESP-IDF and export IDF_PATH}"
node=${1:?usage: flash.sh bag-node|bin-node|inspection-beacon}
case "$node" in bag-node|bin-node) ;; *) echo "This script only targets ESP-IDF nodes; beacon toolchain setup is manual" >&2; exit 2;; esac
cd "$(dirname "$0")/../firmware/$node"
idf.py build
