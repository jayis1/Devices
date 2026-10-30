#!/bin/sh
# Prototype deployment placeholder; requires operator-provisioned secrets and TLS.
set -eu
: "${MQTT_URL:?Set MQTT_URL explicitly}"
printf '%s\n' 'Configure broker ACLs/TLS and deploy the API using reviewed infrastructure.'
