#!/bin/sh
set -eu
: "${MQTT_URL:?Set MQTT_URL to secured broker URL}"
: "${MQTT_TOPIC:=cupcycle/v1/site/node/events}"
printf 'Configure backend with MQTT_URL and MQTT_TOPIC=%s\n' "$MQTT_TOPIC"
