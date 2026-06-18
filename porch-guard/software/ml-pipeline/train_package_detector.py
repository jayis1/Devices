"""
train_package_detector.py — Train package detection model (on-device, TFLite Micro)

Detects parcels on the porch from OV2640 frames (320x240). MobileNet-SSD lite
architecture, INT8 quantized, ~210 KB. Deployed on the porch camera (ESP32-S3).

Classes: no-parcel, small-parcel, medium-parcel, large-parcel, envelope

Usage:
    python3 train_package_detector.py --epochs 60
    python3 train_package_detector.py --epochs 60 --samples 10000
"""

import argparse
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, Model
from sklearn.model_selection import train_test_split

IMG_SIZE = 160   # downsampled from 320x240 for training efficiency
CHANNELS = 3

PACKAGE_CLASSES = ['no-parcel', 'small-parcel', 'medium-parcel',
                   'large-parcel', 'envelope']


def build_package_detector(input_size=IMG_SIZE):
    """MobileNet-SSD-lite style detector for parcel classification + bbox.
    Classification head only for the on-device model (bbox regression kept simple)."""
    base = tf.keras.applications.MobileNetV3Small(
        input_shape=(input_size, input_size, CHANNELS),
        include_top=False,
        weights=None,
        minimal_level=1,
    )
    x = layers.GlobalAveragePooling2D()(base.output)
    x = layers.Dropout(0.25)(x)
    x = layers.Dense(64, activation='relu')(x)
    # Classification: 5 parcel classes
    cls_out = layers.Dense(len(PACKAGE_CLASSES), activation='softmax',
                           name='cls')(x)
    # Bbox: [x, y, w, h] normalized 0-1
    bbox_out = layers.Dense(4, activation='sigmoid', name='bbox')(x)
    model = Model(base.input, [cls_out, bbox_out])
    model.compile(
        optimizer='adam',
        loss={'cls': 'sparse_categorical_crossentropy', 'bbox': 'mse'},
        loss_weights={'cls': 1.0, 'bbox': 0.5},
        metrics={'cls': 'accuracy'},
    )
    return model


def generate_package_data(n_samples=8000, img_size=IMG_SIZE):
    """Synthetic porch images with parcels.

    Parcel visual profiles (color ranges in RGB):
    - no-parcel: bare porch (wood/concrete texture, no box)
    - small-parcel: ~30px box, brown/cardboard
    - medium-parcel: ~60px box, brown or white
    - large-parcel: ~100px box, brown
    - envelope: thin flat rectangle, white/manila

    Each sample: image + class label + bbox [x,y,w,h] normalized.
    """
    np.random.seed(42)
    X = np.zeros((n_samples, img_size, img_size, CHANNELS))
    y_cls = np.zeros(n_samples, dtype=int)
    y_bbox = np.zeros((n_samples, 4), dtype=np.float32)

    # Porch background: brown/gray gradient + noise
    bg_palette = [
        (140, 110, 80),   # wood
        (170, 170, 160),  # concrete
        (120, 90, 70),    # dark wood
    ]

    # Parcel profiles (color, typical size fraction)
    parcel_profiles = {
        0: None,                          # no-parcel
        1: ((160, 120, 80), 0.15),         # small brown
        2: ((180, 140, 100), 0.28),        # medium brown
        3: ((200, 170, 130), 0.45),        # large
        4: ((230, 220, 190), 0.12),        # envelope manila
    }

    for i in range(n_samples):
        label = np.random.randint(0, len(PACKAGE_CLASSES))
        bg = bg_palette[np.random.randint(0, len(bg_palette))]
        # Gradient background
        grad = np.linspace(0.85, 1.0, img_size)
        for c in range(CHANNELS):
            X[i, :, :, c] = (bg[c] * grad[np.newaxis, :]
                             + np.random.normal(0, 8, (img_size, img_size)))
        X[i] = np.clip(X[i], 0, 255)

        if label == 0:
            # No parcel — bbox = 0
            y_bbox[i] = 0.0
        else:
            color, size_frac = parcel_profiles[label]
            box_size = int(img_size * size_frac * np.random.uniform(0.8, 1.2))
            box_size = max(8, min(img_size - 4, box_size))
            x0 = np.random.randint(2, img_size - box_size - 2)
            y0 = np.random.randint(2, img_size - box_size - 2)
            # Fill box region with color + texture
            X[i, y0:y0+box_size, x0:x0+box_size, :] = color
            X[i, y0:y0+box_size, x0:x0+box_size, :] += np.random.normal(
                0, 12, (box_size, box_size, CHANNELS))
            # Add darker edge (cardboard seam)
            X[i, y0:y0+box_size, x0:x0+2, :] *= 0.7
            X[i, y0:y0+box_size, x0+box_size-2:x0+box_size, :] *= 0.7
            X[i, y0:y0+2, x0:x0+box_size, :] *= 0.7
            X[i, y0+box_size-2:y0+box_size, x0:x0+box_size, :] *= 0.7
            X[i] = np.clip(X[i], 0, 255)
            # Normalized bbox
            y_bbox[i] = [x0 / img_size, y0 / img_size,
                         box_size / img_size, box_size / img_size]
        y_cls[i] = label

    return X / 255.0, y_cls, y_bbox


def train_model(model, X_train, y_cls_train, y_bbox_train,
                X_test, y_cls_test, y_bbox_test, epochs):
    print("\n--- Training Package Detector ---")
    history = model.fit(
        X_train, {'cls': y_cls_train, 'bbox': y_bbox_train},
        validation_split=0.2,
        epochs=epochs,
        batch_size=32,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=8, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=4),
        ]
    )
    results = model.evaluate(X_test,
                              {'cls': y_cls_test, 'bbox': y_bbox_test},
                              verbose=0)
    print(f"\nPackage Detector — Test results: {results}")
    return model


def quantize_tflite(model, X_train, output_path):
    """Convert to TFLite INT8 for ESP32-S3 deployment."""
    def representative_data():
        for i in range(min(200, len(X_train))):
            yield [X_train[i:i+1].astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_data
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite = converter.convert()
    with open(output_path, 'wb') as f:
        f.write(tflite)
    print(f"TFLite INT8 saved: {output_path} ({len(tflite)/1024:.1f} KB)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=8000)
    parser.add_argument("--epochs", type=int, default=60)
    args = parser.parse_args()

    print("Generating package data...")
    X, y_cls, y_bbox = generate_package_data(args.samples)
    X_tr, X_te, ycls_tr, ycls_te, ybbox_tr, ybbox_te = train_test_split(
        X, y_cls, y_bbox, test_size=0.2, random_state=42)

    model = build_package_detector()
    model = train_model(model, X_tr, ycls_tr, ybbox_tr, X_te, ycls_te, ybbox_te,
                        args.epochs)
    quantize_tflite(model, X_tr, "package_detector.tflite")


if __name__ == "__main__":
    main()