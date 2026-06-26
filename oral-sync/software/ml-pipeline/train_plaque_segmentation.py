"""
Model 2: Plaque-segmentation U-Net-tiny
  Input:  405 nm fluorescence image 256×256 (plaque porphyrins fluoresce red-orange)
  Output: per-pixel plaque mask (binary) → plaque % per tooth surface
  Arch:   U-Net-tiny (4 encoder/decoder levels, 16→32→64→128 channels)
  Export: TFLite int8 for ESP32-S3 (<500 KB)
"""
import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

H = W = 256

def conv_block(x, ch):
    x = layers.Conv2D(ch, 3, padding="same", activation="relu")(x)
    x = layers.Conv2D(ch, 3, padding="same", activation="relu")(x)
    return x

def build_unet_tiny():
    inp = keras.Input((H, W, 1))
    c1 = conv_block(inp, 16);   p1 = layers.MaxPool2D()(c1)
    c2 = conv_block(p1, 32);    p2 = layers.MaxPool2D()(c2)
    c3 = conv_block(p2, 64);    p3 = layers.MaxPool2D()(c3)
    c4 = conv_block(p3, 128)
    u3 = layers.Conv2DTranspose(64, 2, strides=2, padding="same")(c4)
    u3 = layers.Concatenate()([u3, c3]); cu3 = conv_block(u3, 64)
    u2 = layers.Conv2DTranspose(32, 2, strides=2, padding="same")(cu3)
    u2 = layers.Concatenate()([u2, c2]); cu2 = conv_block(u2, 32)
    u1 = layers.Conv2DTranspose(16, 2, strides=2, padding="same")(cu2)
    u1 = layers.Concatenate()([u1, c1]); cu1 = conv_block(u1, 16)
    out = layers.Conv2D(1, 1, activation="sigmoid")(cu1)
    return keras.Model(inp, out)

def load_data():
    try:
        X = np.load("data/plaque/X.npy").astype(np.float32) / 255.0
        y = np.load("data/plaque/y.npy").astype(np.float32)
    except FileNotFoundError:
        print("[warn] data/plaque/* not found — synthetic placeholder")
        rng = np.random.default_rng(42)
        X = rng.random((400, H, W, 1)).astype(np.float32)
        y = (rng.random((400, H, W, 1)) > 0.7).astype(np.float32)
    return X, y

def export_tflite_int8(model, X_train):
    def rep():
        for i in range(min(100, len(X_train))):
            yield [X_train[i:i+1]]
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = rep
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    buf = conv.convert()
    with open("artifacts/plaque_unet_tiny_int8.tflite", "wb") as f:
        f.write(buf)
    print(f"✓ Exported artifacts/plaque_unet_tiny_int8.tflite ({len(buf)} bytes)")

def main():
    X, y = load_data()
    split = int(len(X) * 0.8)
    X_tr, y_tr = X[:split], y[:split]
    X_te, y_te = X[split:], y[split:]
    model = build_unet_tiny()
    model.compile(optimizer="adam", loss="binary_crossentropy", metrics=["accuracy"])
    model.fit(X_tr, y_tr, epochs=25, batch_size=8, validation_split=0.1, verbose=2)
    _, acc = model.evaluate(X_te, y_te, verbose=0)
    print(f"Test pixel accuracy: {acc:.3f}")
    export_tflite_int8(model, X_tr)

if __name__ == "__main__":
    main()