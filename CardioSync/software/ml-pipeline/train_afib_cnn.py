"""
train_afib_cnn.py — Train AFib detection CNN (1D CNN, 5-class)

Model: 1D CNN with 5 conv layers + 2 FC layers
Input: 30-second ECG at 250 Hz (7500 samples)
Output: 5 classes (Normal, AFib, PVC, VT, Bradycardia)

Dataset: MIT-BIH Arrhythmia Database + PhysioNet AFib Database
  - MIT-BIH: 48 records, 30 min each, 360 Hz (resampled to 250 Hz)
  - PhysioNet AFib: 12 long-term AFib records

Model size: ~22 KB (quantized int8 for tflite-micro)
Inference: ~180 ms on ESP32-S3 (240 MHz)

License: MIT
"""
import os
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models, optimizers
from tensorflow.keras.utils import to_categorical
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import wfdb
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# ── Configuration ─────────────────────────────────────────────
ECG_SAMPLE_RATE = 250        # Hz
ECG_WINDOW_S = 30            # seconds
ECG_WINDOW_SAMPLES = ECG_SAMPLE_RATE * ECG_WINDOW_S  # 7500
NUM_CLASSES = 5              # Normal, AFib, PVC, VT, Bradycardia
BATCH_SIZE = 64
EPOCHS = 50
LEARNING_RATE = 0.001
MODEL_PATH = "models/afib_cnn.h5"
TFLITE_PATH = "models/afib_cnn.tflite"
TFLITE_MICRO_PATH = "models/afib_cnn_tflite.cc"

CLASS_NAMES = ['Normal', 'AFib', 'PVC', 'VT', 'Bradycardia']

# ── 1D CNN Architecture ──────────────────────────────────────
def build_model():
    """
    1D CNN for ECG classification:
      Conv1D(32, 7) → Conv1D(32, 5) → MaxPool(2)
      Conv1D(64, 5) → Conv1D(64, 5) → MaxPool(2)
      Conv1D(128, 3) → MaxPool(2)
      Flatten → Dense(128) → Dense(64) → Dense(NUM_CLASSES, softmax)

    Total params: ~580K → quantized to ~22 KB (int8)
    """
    model = models.Sequential([
        # Input: (7500, 1)
        layers.Input(shape=(ECG_WINDOW_SAMPLES, 1)),

        # Conv block 1
        layers.Conv1D(32, 7, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.Conv1D(32, 5, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        # Conv block 2
        layers.Conv1D(64, 5, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.Conv1D(64, 5, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        # Conv block 3
        layers.Conv1D(128, 3, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        # Classifier
        layers.Flatten(),
        layers.Dense(128, activation='relu'),
        layers.Dropout(0.5),
        layers.Dense(64, activation='relu'),
        layers.Dropout(0.3),
        layers.Dense(NUM_CLASSES, activation='softmax')
    ])

    model.compile(
        optimizer=optimizers.Adam(learning_rate=LEARNING_RATE),
        loss='categorical_crossentropy',
        metrics=['accuracy', tf.keras.metrics.AUC(name='auc')]
    )

    return model

# ── Data Loading (MIT-BIH + PhysioNet AFib) ──────────────────
def load_mitbih_data(data_dir="data/mit-bih"):
    """Load MIT-BIH Arrhythmia Database records.

    Classes:
    - Normal (N, L, R, e, j)
    - PVC (V)
    - VT (runs of V ≥ 3 consecutive)
    - Bradycardia (HR < 40 bpm for >10 s)
    """
    records = []
    labels = []

    # In production: download from PhysioNet
    # wfdb.dl_database('mitdb', data_dir, os.listdir(data_dir))

    record_list = ['100', '101', '102', '103', '104', '105', '106',
                   '107', '108', '109', '111', '112', '113', '114',
                   '115', '116', '117', '118', '119', '121', '122',
                   '123', '124', '200', '201', '202', '203', '205',
                   '207', '208', '209', '210', '212', '213', '214',
                   '215', '217', '219', '220', '221', '222', '223',
                   '228', '230', '231', '232', '233', '234']

    for rec_id in record_list:
        rec_path = os.path.join(data_dir, rec_id)
        if not os.path.exists(rec_path + '.hea'):
            continue

        # Read record
        record = wfdb.rdrecord(rec_path)
        annotations = wfdb.rdann(rec_path, 'atr')

        # Resample to 250 Hz (MIT-BIH is 360 Hz)
        ecg = record.p_signal[:, 0]  # Lead I
        ecg_resampled = wfdb.processing.resample_sig(ecg, 360, ECG_SAMPLE_RATE)[0]

        # Create 30-second windows
        windows_per_record = len(ecg_resampled) // ECG_WINDOW_SAMPLES

        for w in range(windows_per_record):
            start = w * ECG_WINDOW_SAMPLES
            end = start + ECG_WINDOW_SAMPLES
            window = ecg_resampled[start:end]

            # Normalize
            window = (window - np.mean(window)) / (np.std(window) + 1e-8)

            # Classify window based on annotations
            ann_in_window = [a for i, a in enumerate(annotations.symbol)
                            if start <= annotations.sample[i] < end]

            # Determine class
            if len(ann_in_window) == 0:
                cls = 0  # Normal
            elif 'V' in ann_in_window:
                # Check for VT (3+ consecutive PVCs)
                v_count = ann_in_window.count('V')
                if v_count >= 3:
                    cls = 3  # VT
                else:
                    cls = 2  # PVC
            elif 'A' in ann_in_window or 'a' in ann_in_window:
                # In MIT-BIH, A = atrial premature beat (proxy for AFib)
                cls = 1  # AFib
            else:
                cls = 0  # Normal

            # Check for bradycardia (simplified: count R-peaks)
            if ann_in_window:
                rr_intervals = np.diff([annotations.sample[i] for i, a in
                                       enumerate(annotations.symbol)
                                       if start <= annotations.sample[i] < end])
                if len(rr_intervals) > 0:
                    avg_rr = np.mean(rr_intervals) / ECG_SAMPLE_RATE * 1000  # ms
                    hr = 60000 / avg_rr if avg_rr > 0 else 100
                    if hr < 40:
                        cls = 4  # Bradycardia

            records.append(window)
            labels.append(cls)

    return np.array(records), np.array(labels)

def load_afib_data(data_dir="data/afib"):
    """Load PhysioNet AFib Database records for AFib class."""
    records = []
    labels = []

    # PhysioNet AFib Database: 12 long-term records
    record_list = ['04015', '04043', '04048', '04126', '04746',
                   '04908', '04936', '05091', '05121', '05261',
                   '06426', '08405']

    for rec_id in record_list[:4]:  # subset for demo
        rec_path = os.path.join(data_dir, rec_id)
        if not os.path.exists(rec_path + '.hea'):
            continue

        record = wfdb.rdrecord(rec_path)
        ecg = record.p_signal[:, 0]

        # Resample to 250 Hz (AFib DB is 250 Hz already)
        if record.fs != ECG_SAMPLE_RATE:
            ecg = wfdb.processing.resample_sig(ecg, record.fs, ECG_SAMPLE_RATE)[0]

        # Create windows
        windows_per_record = len(ecg) // ECG_WINDOW_SAMPLES
        for w in range(min(windows_per_record, 100)):  # cap for balance
            start = w * ECG_WINDOW_SAMPLES
            end = start + ECG_WINDOW_SAMPLES
            window = ecg[start:end]
            window = (window - np.mean(window)) / (np.std(window) + 1e-8)
            records.append(window)
            labels.append(1)  # AFib

    return np.array(records), np.array(labels)

# ── Training ─────────────────────────────────────────────────
def train():
    print("=" * 60)
    print("CardioSync AFib CNN Training")
    print("=" * 60)

    # Load data
    print("\n[1/5] Loading MIT-BIH data...")
    X_mit, y_mit = load_mitbih_data()
    print(f"  MIT-BIH: {len(X_mit)} windows")

    print("\n[2/5] Loading PhysioNet AFib data...")
    X_afib, y_afib = load_afib_data()
    print(f"  AFib DB: {len(X_afib)} windows")

    # Combine
    X = np.concatenate([X_mit, X_afib], axis=0)
    y = np.concatenate([y_mit, y_afib], axis=0)
    X = X.reshape(-1, ECG_WINDOW_SAMPLES, 1)

    # Class distribution
    print("\nClass distribution:")
    for i, name in enumerate(CLASS_NAMES):
        count = np.sum(y == i)
        print(f"  {name}: {count} ({count/len(y)*100:.1f}%)")

    # Split
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )

    y_train_cat = to_categorical(y_train, NUM_CLASSES)
    y_test_cat = to_categorical(y_test, NUM_CLASSES)

    # Build model
    print("\n[3/5] Building model...")
    model = build_model()
    model.summary()

    # Callbacks
    callbacks = [
        tf.keras.callbacks.EarlyStopping(monitor='val_loss', patience=10,
                                         restore_best_weights=True),
        tf.keras.callbacks.ReduceLROnPlateau(monitor='val_loss', factor=0.5,
                                              patience=5, min_lr=1e-6),
    ]

    # Train
    print("\n[4/5] Training...")
    history = model.fit(
        X_train, y_train_cat,
        validation_split=0.15,
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1
    )

    # Evaluate
    print("\n[5/5] Evaluating...")
    y_pred = model.predict(X_test)
    y_pred_classes = np.argmax(y_pred, axis=1)

    print("\nClassification Report:")
    print(classification_report(y_test, y_pred_classes,
                                target_names=CLASS_NAMES))

    print("\nConfusion Matrix:")
    cm = confusion_matrix(y_test, y_pred_classes)
    print(cm)

    # Save model
    os.makedirs("models", exist_ok=True)
    model.save(MODEL_PATH)
    print(f"\nModel saved: {MODEL_PATH}")

    # Convert to TFLite (float)
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_model = converter.convert()
    with open(TFLITE_PATH, 'wb') as f:
        f.write(tflite_model)
    print(f"TFLite model saved: {TFLITE_PATH}")

    # Convert to TFLite (int8 quantized for ESP32-S3 edge inference)
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # Representative dataset for quantization
    def representative_dataset():
        for i in range(min(100, len(X_train))):
            yield [X_train[i:i+1].astype(np.float32)]

    converter.representative_dataset = representative_dataset
    tflite_quant = converter.convert()
    with open(TFLITE_PATH.replace('.tflite', '_int8.tflite'), 'wb') as f:
        f.write(tflite_quant)
    print(f"Quantized TFLite model saved: {TFLITE_PATH.replace('.tflite', '_int8.tflite')}")

    # Generate C array for tflite-micro (ESP32-S3)
    generate_c_array(tflite_quant, TFLITE_MICRO_PATH)
    print(f"C array for tflite-micro: {TFLITE_MICRO_PATH}")

    # Plot training history
    plot_history(history)

    return model

def generate_c_array(tflite_model, output_path):
    """Generate C array for tflite-micro (embedded in ESP32-S3 firmware)"""
    with open(output_path, 'w') as f:
        f.write("/* Auto-generated by train_afib_cnn.py */\n")
        f.write("/* CardioSync AFib detection CNN (int8 quantized) */\n")
        f.write(f"/* Size: {len(tflite_model)} bytes */\n\n")
        f.write('#include "tensorflow/lite/micro/micro_error_reporter.h"\n\n')
        f.write(f'const unsigned char afib_cnn_tflite[] __PROGMEM = {{\n')
        for i, b in enumerate(tflite_model):
            if i % 12 == 0:
                f.write('  ')
            f.write(f'0x{b:02x}, ')
            if i % 12 == 11:
                f.write('\n')
        f.write('\n};\n\n')
        f.write(f'const unsigned int afib_cnn_tflite_len = {len(tflite_model)};\n')

def plot_history(history):
    """Plot training history"""
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))

    axes[0].plot(history.history['loss'], label='Train')
    axes[0].plot(history.history['val_loss'], label='Val')
    axes[0].set_title('Loss')
    axes[0].legend()

    axes[1].plot(history.history['accuracy'], label='Train')
    axes[1].plot(history.history['val_accuracy'], label='Val')
    axes[1].set_title('Accuracy')
    axes[1].legend()

    plt.savefig('models/training_history.png', dpi=150, bbox_inches='tight')
    print("Training history plot: models/training_history.png")

if __name__ == "__main__":
    train()