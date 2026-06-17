# SoundNest ML Model Export

This directory contains the exported TinyML models for deployment to SoundNest nodes.

## Model Architecture

- **Sound Classifier**: MobileNet-inspired depthwise-separable CNN
- **Input**: 40-band mel-spectrogram, 2-second windows at 16kHz
- **Output**: 40-class softmax probabilities
- **Size**: ~200KB (int8 quantized)
- **Latency**: 20ms on nRF52840, 5ms on ESP32-S3

## Exported Files

- `sound_classifier_int8.tflite` — TFLite Micro model for nRF52840
- `sound_classifier_espnn.bin` — ESP-NN optimized model for ESP32-S3
- `model_metadata.json` — Model metadata and class labels

## Training

See `../training/train_classifier.py` for the training pipeline.

## Deployment

### nRF52840 (Room Sensor)
```bash
# Convert TFLite model to C array
xxd -i sound_classifier_int8.tflite > sound_classifier_int8.cc
```

### ESP32-S3 (Hub Node)
```bash
# ESP-NN optimized model
python3 export_espnn.py --model sound_classifier_int8.tflite --output sound_classifier_espnn.bin
```