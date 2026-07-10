"""
AllergySync — PollenForecast LSTM Model
=======================================
24-hour pollen concentration forecast.

Input features (per timestep, 72 hours history):
  - PM2.5, PM10 (from sentinel)
  - Pollen class (one-hot, 6)
  - Temperature, humidity, pressure
  - Wind speed, wind direction (from weather API)
  - Day-of-year (sin/cos encoded)
  - Hour-of-day (sin/cos encoded)

Output: 24-hour forecast of pollen concentration per class.

Architecture:
  Input(72, 14) → LSTM(64, return_sequences=True) → LSTM(32)
  → Dense(32) → Dense(24 × 6) → reshape → Softmax per timestep
"""

import tensorflow as tf
import numpy as np

SEQ_LEN = 72      # 72 hours of history
N_FEATURES = 14   # PM + weather + temporal
N_CLASSES = 6     # pollen types
FORECAST_HOURS = 24

def build_pollen_forecast():
    """Build the PollenForecast LSTM model."""
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(SEQ_LEN, N_FEATURES), name="history"),
        tf.keras.layers.LSTM(64, return_sequences=True, name="lstm1"),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.LSTM(32, name="lstm2"),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(32, activation="relu", name="dense1"),
        tf.keras.layers.Dense(FORECAST_HOURS * N_CLASSES, name="output"),
        tf.keras.layers.Reshape((FORECAST_HOURS, N_CLASSES), name="forecast"),
        tf.keras.layers.Softmax(axis=-1, name="softmax"),
    ])

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss="categorical_crossentropy",
        metrics=["accuracy"]
    )

    model.summary()
    return model


def generate_synthetic_weather_data(n_samples=1000, seq_len=SEQ_LEN):
    """
    Generate synthetic time-series data for training.
    In production, this comes from the database + Open-Meteo API.
    """
    rng = np.random.default_rng(42)

    X = np.zeros((n_samples, seq_len, N_FEATURES), dtype=np.float32)
    y = np.zeros((n_samples, FORECAST_HOURS, N_CLASSES), dtype=np.float32)

    for i in range(n_samples):
        # Simulate 72 hours of data
        base_pollen = rng.uniform(0, 100)
        # Seasonal component (sine wave over year)
        day_of_year = rng.integers(1, 365)
        seasonal = np.sin(2 * np.pi * day_of_year / 365) * 50 + 50

        for t in range(seq_len):
            hour = t % 24
            # Daily cycle: pollen peaks 6-10 AM
            daily = np.sin(2 * np.pi * (hour - 6) / 24) * 20 + 20
            pollen = max(0, seasonal + daily + rng.normal(0, 10))

            X[i, t, 0] = pollen * 0.1  # PM2.5
            X[i, t, 1] = pollen * 0.2  # PM10
            # One-hot pollen class (random for synthetic)
            cls = rng.integers(0, N_CLASSES)
            for c in range(N_CLASSES):
                X[i, t, 2 + c] = 1.0 if c == cls else 0.0
            X[i, t, 8] = 15 + rng.normal(0, 5)    # temp °C
            X[i, t, 9] = 50 + rng.normal(0, 10)  # humidity %
            X[i, t, 10] = 1013 + rng.normal(0, 5)  # pressure hPa
            X[i, t, 11] = max(0, 5 + rng.normal(0, 3))  # wind speed m/s
            X[i, t, 12] = np.sin(2 * np.pi * day_of_year / 365)  # DoY sin
            X[i, t, 13] = np.cos(2 * np.pi * day_of_year / 365)  # DoY cos

        # Generate forecast labels (next 24 hours)
        for t in range(FORECAST_HOURS):
            hour = (seq_len + t) % 24
            daily = np.sin(2 * np.pi * (hour - 6) / 24) * 20 + 20
            pollen = max(0, seasonal + daily + rng.normal(0, 10))
            # Distribute across classes
            probs = rng.dirichlet(np.ones(N_CLASSES) * pollen / 10 + 0.1)
            y[i, t] = probs

    return X, y


if __name__ == "__main__":
    model = build_pollen_forecast()

    X, y = generate_synthetic_weather_data(2000)
    print(f"Training data: X={X.shape}, y={y.shape}")

    split = int(0.8 * len(X))
    model.fit(X[:split], y[:split], validation_data=(X[split:], y[split:]),
              epochs=30, batch_size=16, verbose=1)

    loss, acc = model.evaluate(X[split:], y[split:], verbose=0)
    print(f"Validation accuracy: {acc:.4f}")

    model.save("pollen_forecast_lstm.keras")
    print("Model saved: pollen_forecast_lstm.keras")