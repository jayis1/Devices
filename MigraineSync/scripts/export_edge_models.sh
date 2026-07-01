#!/bin/bash
# MigraineSync — Export Edge Models for Hub
# ==========================================
# Exports quantized tflite-micro models from trained Keras models
# and packages them for OTA deployment to the ESP32-S3 hub.
#
# License: MIT

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ML_DIR="$SCRIPT_DIR/../software/ml-pipeline"
MODEL_DIR="$ML_DIR/../models"
EDGE_DIR="$SCRIPT_DIR/../firmware/hub/models"

mkdir -p "$EDGE_DIR"

echo "============================================"
echo "  MigraineSync — Export Edge Models"
echo "============================================"

# Check for trained models
if [ ! -f "$MODEL_DIR/onset_predictor.tflite" ]; then
    echo "ERROR: onset_predictor.tflite not found."
    echo "Run scripts/train_all.sh first."
    exit 1
fi

if [ ! -f "$MODEL_DIR/prodrome_detector.tflite" ]; then
    echo "ERROR: prodrome_detector.tflite not found."
    echo "Run scripts/train_all.sh first."
    exit 1
fi

# Copy tflite models to firmware directory
echo "Copying quantized models to firmware/hub/models/..."
cp "$MODEL_DIR/onset_predictor.tflite" "$EDGE_DIR/"
cp "$MODEL_DIR/prodrome_detector.tflite" "$EDGE_DIR/"

# Copy normalization parameters
if [ -f "$MODEL_DIR/onset_norm_mean.npy" ]; then
    cp "$MODEL_DIR/onset_norm_mean.npy" "$EDGE_DIR/"
    cp "$MODEL_DIR/onset_norm_std.npy" "$EDGE_DIR/"
fi
if [ -f "$MODEL_DIR/prodrome_norm_mean.npy" ]; then
    cp "$MODEL_DIR/prodrome_norm_mean.npy" "$EDGE_DIR/"
    cp "$MODEL_DIR/prodrome_norm_std.npy" "$EDGE_DIR/"
fi

# Generate C header for embedding models in firmware
python3 -c "
import os
models = ['onset_predictor', 'prodrome_detector']
for name in models:
    path = os.path.join('$EDGE_DIR', name + '.tflite')
    if os.path.exists(path):
        with open(path, 'rb') as f:
            data = f.read()
        header_path = os.path.join('$EDGE_DIR', name + '_model.h')
        with open(header_path, 'w') as h:
            h.write(f'// Auto-generated: {name} tflite model ({len(data)} bytes)\n')
            h.write(f'const unsigned int {name}_model_len = {len(data)};\n')
            h.write(f'const unsigned char {name}_model[] = {{\n')
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                h.write('  ' + ','.join(f'0x{b:02x}' for b in chunk) + ',\n')
            h.write('};\n')
        print(f'  {name}: {len(data)} bytes → {header_path}')
"

echo ""
echo "============================================"
echo "  Edge models exported successfully!"
echo "  Location: $EDGE_DIR"
echo ""
echo "  To deploy via OTA:"
echo "    1. Flash hub with new firmware containing models"
echo "    2. Or use BLE OTA update from mobile app"
echo "============================================"