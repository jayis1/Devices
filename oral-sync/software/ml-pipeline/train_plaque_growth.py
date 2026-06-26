"""
Model 5: Plaque-growth LSTM
  Input:  per-tooth weekly plaque % history (12 weeks × 4 features: plaque%, brushing_coverage, pH, diet_proxy)
  Output: 72-hour plaque % forecast per tooth
  Arch:   2-layer LSTM (hidden=64) + Dense
  Export: ONNX for cloud
"""
import numpy as np
import tensorflow as tf
from tensorflow import keras

SEQ = 12        # 12 weeks history
FEAT = 4
HORIZON = 3     # 3 days = 72 h

def load_data():
    try:
        X = np.load("data/plaque_growth/X.npy").astype(np.float32)
        y = np.load("data/plaque_growth/y.npy").astype(np.float32)
    except FileNotFoundError:
        print("[warn] data/plaque_growth/* not found — synthetic placeholder")
        rng = np.random.default_rng(42)
        X = rng.normal(0, 1, (1000, SEQ, FEAT)).astype(np.float32)
        y = rng.normal(20, 10, (1000, HORIZON)).astype(np.float32)
    return X, y

def build_model():
    m = keras.Sequential([
        keras.layers.Input((SEQ, FEAT)),
        keras.layers.LSTM(64, return_sequences=True),
        keras.layers.LSTM(64),
        keras.layers.Dense(32, activation="relu"),
        keras.layers.Dense(HORIZON),
    ])
    m.compile(optimizer="adam", loss="mse", metrics=["mae"])
    return m

def main():
    X, y = load_data()
    split = int(len(X) * 0.8)
    model = build_model()
    model.fit(X[:split], y[:split], epochs=40, batch_size=32, validation_split=0.1, verbose=2)
    _, mae = model.evaluate(X[split:], y[split:], verbose=0)
    print(f"Test MAE: {mae:.2f} plaque %")
    # Export ONNX
    import tf2onnx, onnx
    spec = (tf.TensorSpec((None, SEQ, FEAT), tf.float32, name="plaque_history"),)
    onnx_model, _ = tf2onnx.convert.from_keras(model, input_signature=spec, output_path="artifacts/plaque_growth_lstm.onnx")
    print("✓ Exported artifacts/plaque_growth_lstm.onnx")

if __name__ == "__main__":
    main()