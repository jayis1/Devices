"""
Model 1: Brushing-technique classifier
  Input:  IMU 6-DoF @ 50 Hz, 2-second windows (100 samples × 6 channels)
  Output: 8 classes — Bass, Modified-Bass, Fones, Stillman, Scrub, Charter, Floss-pick, Sonic
  Arch:   1D CNN (3× Conv1D + Dense) — exported to TFLite Micro int8 (<80 KB) for RP2040
"""
import numpy as np
import tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

WINDOW = 100        # 2 s @ 50 Hz
CHANNELS = 6        # ax,ay,az,gx,gy,gz
N_CLASSES = 8
CLASS_NAMES = ["Bass","Modified-Bass","Fones","Stillman","Scrub","Charter","Floss-pick","Sonic"]

def load_data():
    """Load preprocessed IMU windows from data/brushing/X.npy, y.npy.
    Each X: (WINDOW, CHANNELS) int16. Synthetic placeholder if absent."""
    try:
        X = np.load("data/brushing/X.npy").astype(np.float32)
        y = np.load("data/brushing/y.npy").astype(np.int32)
    except FileNotFoundError:
        print("[warn] data/brushing/* not found — generating synthetic placeholder")
        rng = np.random.default_rng(42)
        X = rng.normal(0, 500, (800, WINDOW, CHANNELS)).astype(np.float32)
        y = rng.integers(0, N_CLASSES, 800).astype(np.int32)
    return X, y

def build_model():
    m = keras.Sequential([
        keras.layers.Input(shape=(WINDOW, CHANNELS)),
        keras.layers.Conv1D(16, 5, activation="relu", padding="same"),
        keras.layers.Conv1D(32, 5, activation="relu", padding="same"),
        keras.layers.MaxPool1D(2),
        keras.layers.Conv1D(64, 3, activation="relu", padding="same"),
        keras.layers.GlobalAveragePooling1D(),
        keras.layers.Dense(32, activation="relu"),
        keras.layers.Dense(N_CLASSES, activation="softmax"),
    ])
    m.compile(optimizer="adam", loss="sparse_categorical_crossentropy", metrics=["accuracy"])
    return m

def export_tflite_int8(model, X_train):
    """Export to TFLite int8 for TFLite Micro on RP2040 (<80 KB)."""
    def rep():
        for i in range(min(200, len(X_train))):
            yield [X_train[i:i+1]]
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = rep
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    buf = conv.convert()
    with open("artifacts/brushing_technique_int8.tflite", "wb") as f:
        f.write(buf)
    print(f"✓ Exported artifacts/brushing_technique_int8.tflite ({len(buf)} bytes)")

def main():
    X, y = load_data()
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)
    model = build_model()
    model.fit(X_train, y_train, epochs=30, batch_size=32, validation_split=0.1, verbose=2)
    _, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"Test accuracy: {acc:.3f}")
    y_pred = model.predict(X_test, verbose=0).argmax(1)
    print(classification_report(y_test, y_pred, target_names=CLASS_NAMES))
    export_tflite_int8(model, X_train)

if __name__ == "__main__":
    main()