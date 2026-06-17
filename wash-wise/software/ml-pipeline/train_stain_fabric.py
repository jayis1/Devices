"""
train_stain_fabric.py — Train stain + fabric classification models

Two models:
  1. Fabric classifier: MobileNetV3-Small, 9 classes (cotton, polyester, wool,
     silk, denim, nylon, linen, blend, unknown). Input: 3-channel multispectral
     (white/UV/IR), 224x224. Output: fabric type + confidence.
  2. Stain classifier: MobileNetV3-Small, 11 classes (clean, coffee, wine, blood,
     grease, grass, ink, food, sweat, rust, unknown). Same multispectral input.

Both deployed as TFLite INT8 for on-scanner inference (~180KB stain, ~85KB fabric).

Usage:
    python3 train_stain_fabric.py --mode fabric --epochs 50
    python3 train_stain_fabric.py --mode stain  --epochs 50
    python3 train_stain_fabric.py --mode both   --epochs 50
"""

import argparse
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, Model
from sklearn.model_selection import train_test_split

IMG_SIZE = 224
CHANNELS = 3  # white, UV, IR

FABRIC_CLASSES = ['unknown', 'cotton', 'polyester', 'wool', 'silk',
                  'denim', 'nylon', 'linen', 'blend']
STAIN_CLASSES = ['clean', 'coffee', 'wine', 'blood', 'grease',
                 'grass', 'ink', 'food', 'sweat', 'rust', 'unknown']


def build_mobilenet_v3(num_classes, input_size=IMG_SIZE):
    """MobileNetV3-Small fine-tuned for multispectral input."""
    base = tf.keras.applications.MobileNetV3Small(
        input_shape=(input_size, input_size, CHANNELS),
        include_top=False,
        weights=None,  # no pretrained for 3-channel custom
        minimal_level=1,
    )
    x = layers.GlobalAveragePooling2D()(base.output)
    x = layers.Dropout(0.2)(x)
    x = layers.Dense(64, activation='relu')(x)
    outputs = layers.Dense(num_classes, activation='softmax')(x)
    model = Model(base.input, outputs)
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    return model


def build_fabric_cnn(num_classes, input_size=64):
    """Lightweight CNN for fabric type — deployable on scanner (ESP32-S3).
    ~85KB INT8. Input: 64x64x3 (downsampled multispectral)."""
    inputs = tf.keras.Input(shape=(input_size, input_size, CHANNELS))
    x = layers.Conv2D(16, 3, activation='relu', padding='same')(inputs)
    x = layers.MaxPooling2D()(x)
    x = layers.Conv2D(32, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling2D()(x)
    x = layers.Conv2D(64, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling2D()(x)
    x = layers.Conv2D(64, 3, activation='relu', padding='same')(x)
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dense(32, activation='relu')(x)
    outputs = layers.Dense(num_classes, activation='softmax')(x)
    model = Model(inputs, outputs)
    model.compile(optimizer='adam', loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    return model


def generate_fabric_data(n_samples=5000, img_size=64):
    """Synthetic multispectral fabric images.

    Each fabric type has distinct multispectral signatures:
    - Cotton: moderate UV fluorescence, visible weave texture
    - Polyester: low UV, smooth
    - Wool: strong UV (protein), fuzzy texture
    - Silk: strong UV, smooth/shiny
    - Denim: low UV, heavy twill pattern
    """
    np.random.seed(42)
    X = np.zeros((n_samples, img_size, img_size, CHANNELS))
    y = np.zeros(n_samples, dtype=int)

    # Spectral profiles per fabric (white, UV, IR mean values)
    profiles = {
        0: [128, 30, 100],  # unknown
        1: [130, 80, 110],   # cotton: mod UV
        2: [140, 25, 95],   # polyester: low UV
        3: [120, 120, 90],  # wool: high UV (protein)
        4: [160, 130, 85],  # silk: high UV, bright
        5: [80, 20, 105],   # denim: dark, low UV
        6: [135, 60, 90],   # nylon: mod UV
        7: [125, 70, 100],  # linen: mod UV
        8: [132, 50, 100],  # blend: mixed
    }

    for i in range(n_samples):
        label = np.random.randint(0, len(FABRIC_CLASSES))
        w, uv, ir = profiles[label]
        # Add texture variation
        texture_freq = np.random.uniform(0.1, 0.5)
        x_grid, y_grid = np.meshgrid(
            np.linspace(0, texture_freq * np.pi, img_size),
            np.linspace(0, texture_freq * np.pi, img_size)
        )
        texture = np.sin(x_grid) * np.cos(y_grid) * 15
        X[i, :, :, 0] = np.clip(w + texture + np.random.normal(0, 10, (img_size, img_size)), 0, 255)
        X[i, :, :, 1] = np.clip(uv + np.random.normal(0, 8, (img_size, img_size)), 0, 255)
        X[i, :, :, 2] = np.clip(ir + np.random.normal(0, 8, (img_size, img_size)), 0, 255)
        y[i] = label

    return X / 255.0, y


def generate_stain_data(n_samples=8000, img_size=64):
    """Synthetic multispectral stain images.

    Stain appearance in different spectra:
    - Coffee/Wine: dark in white, dark in UV, medium IR
    - Blood: dark red in white, BRIGHT in UV (fluoresces), dark IR
    - Grease: translucent in white, dark in UV, VERY dark IR (absorbs)
    - Grass: green in white, moderate UV, medium IR
    - Ink: very dark all spectra
    - Sweat: faint in white, BRIGHT in UV, medium IR
    - Clean: fabric-colored, low UV baseline
    """
    np.random.seed(123)
    X = np.zeros((n_samples, img_size, img_size, CHANNELS))
    y = np.zeros(n_samples, dtype=int)

    # Stain spectral signatures (white, UV, IR) — appears as spot on fabric
    stain_profiles = {
        0: [0, 0, 0],       # clean (no stain)
        1: [40, 30, 80],    # coffee: dark white/UV, med IR
        2: [30, 25, 75],    # wine: dark
        3: [60, 150, 40],   # blood: red, BRIGHT UV
        4: [100, 20, 10],   # grease: translucent, dark UV/IR
        5: [50, 80, 90],    # grass: green, mod UV
        6: [15, 15, 15],    # ink: very dark
        7: [70, 40, 60],    # food: varies
        8: [110, 140, 90],  # sweat: faint, BRIGHT UV
        9: [80, 25, 50],    # rust: orange-brown
        10: [90, 30, 60],   # unknown
    }

    for i in range(n_samples):
        label = np.random.randint(0, len(STAIN_CLASSES))
        # Base fabric (random)
        fabric = np.random.randint(110, 150, (img_size, img_size, CHANNELS))
        if label == 0:  # clean
            X[i] = fabric + np.random.normal(0, 10, fabric.shape)
        else:
            # Add stain blob
            cx, cy = np.random.randint(20, img_size - 20, 2)
            radius = np.random.randint(10, 30)
            yy, xx = np.ogrid[:img_size, :img_size]
            mask = (xx - cx) ** 2 + (yy - cy) ** 2 <= radius ** 2
            sw, suv, sir = stain_profiles[label]
            X[i, :, :, 0] = np.where(mask, sw, fabric[:, :, 0]) + np.random.normal(0, 8, (img_size, img_size))
            X[i, :, :, 1] = np.where(mask, suv, 30) + np.random.normal(0, 8, (img_size, img_size))
            X[i, :, :, 2] = np.where(mask, sir, 100) + np.random.normal(0, 8, (img_size, img_size))
        X[i] = np.clip(X[i], 0, 255)
        y[i] = label

    return X / 255.0, y


def train_model(model, X_train, y_train, X_test, y_test, epochs, name):
    print(f"\n--- Training {name} ---")
    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=epochs,
        batch_size=32,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=8, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=4),
        ]
    )
    loss, acc = model.evaluate(X_test, y_test)
    print(f"\n{name} — Test loss: {loss:.4f}, accuracy: {acc:.4f}")
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
    parser.add_argument("--mode", choices=['fabric', 'stain', 'both'], default='both')
    parser.add_argument("--samples", type=int, default=5000)
    parser.add_argument("--epochs", type=int, default=50)
    args = parser.parse_args()

    if args.mode in ('fabric', 'both'):
        print("Generating fabric data...")
        X, y = generate_fabric_data(args.samples, img_size=64)
        X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42)
        model = build_fabric_cnn(len(FABRIC_CLASSES), input_size=64)
        model = train_model(model, X_tr, y_tr, X_te, y_te, args.epochs, "Fabric CNN")
        quantize_tflite(model, X_tr, "fabric_classifier.tflite")

    if args.mode in ('stain', 'both'):
        print("\nGenerating stain data...")
        X, y = generate_stain_data(args.samples * 2, img_size=64)  # more samples for 11 classes
        X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42)
        model = build_fabric_cnn(len(STAIN_CLASSES), input_size=64)  # reuse architecture
        model = train_model(model, X_tr, y_tr, X_te, y_te, args.epochs, "Stain CNN")
        quantize_tflite(model, X_tr, "stain_classifier.tflite")


if __name__ == "__main__":
    main()