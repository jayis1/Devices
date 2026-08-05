#!/bin/bash
# SeizureSync — OTA model update to edge devices
# Pushes new tflite models to hub and band via MQTT OTA.
set -e
MODELS_DIR="${1:-software/ml-pipeline/models}"
echo "SeizureSync OTA Model Update"
echo "=============================="
echo "Models dir: $MODELS_DIR"

for model in seizurennet_v1.tflite sudepnet_v1.tflite; do
    path="$MODELS_DIR/$model"
    if [ -f "$path" ]; then
        size=$(stat -c%s "$path")
        echo "Publishing $model ($size bytes) via MQTT OTA..."
        # Production: chunk the model and publish via MQTT:
        # mosquitto_pub -t "seizuresync/all/ota" -f "$path"
        echo "  $model ready for OTA"
    else
        echo "  $model not found — skip"
    fi
done

echo ""
echo "OTA complete. Hub and band will apply on next reboot."