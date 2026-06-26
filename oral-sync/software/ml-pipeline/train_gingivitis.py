"""
Model 3: Gingivitis classifier
  Input:  multispectral image 224×224 (red/NIR ratio highlights inflammation)
  Output: 4 classes — Healthy, Mild, Moderate, Severe (Löe-Silness GI 0-3)
  Arch:   MobileNetV3-tiny (transfer learning from ImageNet)
  Export: TFLite (ESP32-S3 preview) + ONNX (cloud full)
"""
import numpy as np
import tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

H = W = 224
N_CLASSES = 4
CLASS_NAMES = ["Healthy","Mild","Moderate","Severe"]

def load_data():
    try:
        X = np.load("data/gingivitis/X.npy").astype(np.float32) / 255.0
        y = np.load("data/gingivitis/y.npy").astype(np.int32)
    except FileNotFoundError:
        print("[warn] data/gingivitis/* not found — synthetic placeholder")
        rng = np.random.default_rng(42)
        X = rng.random((500, H, W, 3)).astype(np.float32)
        y = rng.integers(0, N_CLASSES, 500).astype(np.int32)
    return X, y

def build_model():
    base = keras.applications.MobileNetV3Small(input_shape=(H, W, 3), include_top=False, weights="imagenet")
    base.trainable = False
    m = keras.Sequential([
        base,
        keras.layers.GlobalAveragePooling2D(),
        keras.layers.Dense(64, activation="relu"),
        keras.layers.Dense(N_CLASSES, activation="softmax"),
    ])
    m.compile(optimizer="adam", loss="sparse_categorical_crossentropy", metrics=["accuracy"])
    return m

def main():
    X, y = load_data()
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)
    model = build_model()
    model.fit(X_tr, y_tr, epochs=20, batch_size=16, validation_split=0.1, verbose=2)
    _, acc = model.evaluate(X_te, y_te, verbose=0)
    print(f"Test accuracy: {acc:.3f}")
    y_pred = model.predict(X_te, verbose=0).argmax(1)
    print(classification_report(y_te, y_pred, target_names=CLASS_NAMES))
    # Export TFLite (float16 for ESP32-S3 preview)
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    buf = conv.convert()
    with open("artifacts/gingivitis_mobilenetv3.tflite", "wb") as f:
        f.write(buf)
    print(f"✓ Exported artifacts/gingivitis_mobilenetv3.tflite ({len(buf)} bytes)")

if __name__ == "__main__":
    main()