# SoundNest System Architecture

## Overview

SoundNest is a 4-node AI-powered home acoustic intelligence system that monitors, classifies, masks, and alerts on environmental sounds. It combines 4-microphone array signal processing, TinyML sound classification, adaptive noise masking, and personal sound dose tracking into a unified system.

## System Components

### Hub Node (ESP32-S3 + nRF52840)
- **Role**: Central coordinator, WiFi/BLE bridge, local ML inference
- **Processing**: ESP-NN accelerated 40-class sound classification
- **Communication**: Sub-GHz mesh coordinator, WiFi 6, BLE 5.0 GATT server
- **Display**: 3.5" TFT showing acoustic timeline, SPL gauges, dose meter
- **Audio**: ES8388 codec + 3W speaker for alarms, TTS, tinnitus masking

### Room Acoustic Sensor (nRF52840)
- **Role**: Distributed acoustic monitoring with 4-mic array
- **Processing**: TDOA sound localization, TinyML sound classification
- **Power**: CR123A batteries (6+ months) or USB-C
- **Sensing**: 4× MEMS mic, temp/humidity, light, PIR motion

### Smart Masking Speaker (ESP32-S3)
- **Role**: Adaptive noise masking with stereo directional output
- **Processing**: Real-time noise synthesis (white/pink/brown/nature)
- **Audio**: PCM5102A DAC + 2× 3W speakers in parabolic enclosure
- **Feedback**: Reference microphone for adaptive volume control

### Wearable Sound Tag (nRF52832)
- **Role**: Personal sound dose tracking and haptic alerts
- **Processing**: SPL dosimetry, activity classification
- **Alerts**: ERM haptic motor, RGB LED (color-coded by priority)
- **Power**: 100mAh Li-Po (7 days)

## Data Flow

```
Sound Event Detection Pipeline:

  [4× MEMS Mics] → I2S @ 16kHz → [TDOA Beamforming] → [Direction]
                                              ↓
                                    [Mel-Spectrogram]
                                              ↓
                                    [TinyML Classifier]
                                       (40 classes)
                                              ↓
                                   ┌──────────┼──────────┐
                                   ↓          ↓          ↓
                              [SPL Calc]  [Alert?]  [Mask?]
                                   ↓          ↓          ↓
                              [Hub via    [Wearable [Speaker
                               mesh]       haptic]   masking]
```

## Communication Architecture

### Sub-GHz Mesh (868MHz LoRa)
- **Topology**: Star-with-relay (hub is coordinator)
- **TDMA**: Hub assigns 1-second slots, 10-second superframes
- **Encryption**: AES-128-CCM with per-node keys
- **Range**: 100m indoor (SF7), 500m indoor (SF12), 2km+ LOS

### BLE 5.0 (Mobile App ↔ Hub)
- **Nordic UART Service**: Configuration and data streaming
- **Custom SoundNest Service**: Real-time SPL, events, dose, masking control

### WiFi 6 (Hub ↔ Cloud)
- **MQTT v5 over TLS 1.3**: Events, SPL time-series, dose updates
- **WebSocket**: Real-time push to dashboard

## ML Pipeline

### Sound Classification Model
- **Architecture**: MobileNet-inspired depthwise-separable CNN
- **Input**: 40-band mel-spectrogram, 2-second windows, 16kHz
- **Output**: 40-class softmax probabilities
- **Size**: ~200KB int8 quantized
- **Latency**: 20ms on nRF52840, 5ms on ESP32-S3 (ESP-NN)

### Training Data
- ESC-50 (2,000 clips, 50 classes)
- UrbanSound8K (8,732 clips, 10 classes)
- AudioSet (filtered to relevant classes)
- Custom field recordings from SoundNest prototypes

### Deployment
1. Train PyTorch model on GPU
2. Export to ONNX → TensorFlow → TFLite
3. Quantize to int8 with representative dataset
4. Convert to C array for nRF52840 (TFLite Micro)
5. Optimize for ESP-NN on ESP32-S3

## Sound Dose Calculation

Uses ISO 1999 standard with 3 dB exchange rate:

```
Dose % = (T_actual / T_allowed) × 100%

Where: T_allowed = 8h × 2^((85 dBA - L_A) / 3 dB)

Examples:
- 85 dBA for 8 hours = 100% dose
- 88 dBA for 4 hours = 100% dose
- 100 dBA for 1 hour = 100% dose
- 70 dBA for 8 hours = 3.1% dose
```

## Privacy

- Raw audio is NEVER recorded, stored, or transmitted
- Only ML classification results (class, confidence, direction, SPL) leave the sensor
- All audio processing happens on-device
- Cloud receives only anonymized event summaries
- GDPR compliant by design

## Regulatory Compliance

- IEC 61672: Sound level meter standards for SPL measurement
- OSHA 29 CFR 1910.95: Occupational noise exposure
- WHO Environmental Noise Guidelines
- FCC Part 15: Sub-GHz and BLE radio emissions
- IEC 60601: Medical device considerations for hearing assistance