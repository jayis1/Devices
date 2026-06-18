"""
train_person_reid.py — Train person re-identification model

Produces a 128-d embedding from a small CNN. Deployed on the porch camera
(ESP32-S3, INT8, ~90 KB). Matching via cosine similarity vs gallery of
resident + courier embeddings stored in flash.

Pipeline:
  1. Person detector (MobileNet head) → crop
  2. Re-ID CNN → 128-d embedding (triplet loss)
  3. Cosine similarity vs gallery
  4. Threshold: >0.6 known, <0.4 stranger

Usage:
    python3 train_person_reid.py --epochs 80
    python3 train_person_reid.py --epochs 80 --identities 20 --samples 6000
"""

import argparse
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, Model
from sklearn.model_selection import train_test_split

IMG_SIZE = 96    # person crop is 96x96
CHANNELS = 3
EMBEDDING_DIM = 128


def build_embedding_model(input_size=IMG_SIZE, embed_dim=EMBEDDING_DIM):
    """Small CNN producing 128-d person embedding. ~90 KB INT8."""
    inputs = tf.keras.Input(shape=(input_size, input_size, CHANNELS))
    x = layers.Conv2D(16, 3, strides=2, activation='relu', padding='same')(inputs)
    x = layers.Conv2D(32, 3, strides=2, activation='relu', padding='same')(x)
    x = layers.MaxPooling2D()(x)
    x = layers.Conv2D(64, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling2D()(x)
    x = layers.Conv2D(128, 3, activation='relu', padding='same')(x)
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.3)(x)
    # L2-normalize embedding for cosine similarity
    embedding = layers.Dense(embed_dim, activation=None, name='embedding')(x)
    embedding = tf.keras.layers.Lambda(
        lambda t: tf.math.l2_normalize(t, axis=1))(embedding)
    model = Model(inputs, embedding)
    return model


def build_reid_with_classifier(num_identities, input_size=IMG_SIZE):
    """Re-ID model with a classification head for training (triplet loss is
    hard to converge without a lot of data; use classification + L2 as a
    practical proxy, then strip the head at deployment)."""
    embed_model = build_embedding_model(input_size)
    # Classification head for training only
    cls_out = layers.Dense(num_identities, activation='softmax',
                           name='identity')(embed_model.output)
    model = Model(embed_model.input, cls_out)
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy'],
    )
    return model, embed_model


def generate_person_data(n_identities=20, n_samples=6000, img_size=IMG_SIZE):
    """Synthetic person crops for `n_identities` distinct people.

    Each identity has a characteristic color signature (shirt color + height
    proxy). The model must learn to embed same-identity crops close together.

    In production, you'd use a real person-re-ID dataset (Market-1501,
    MSMT17, or a custom resident gallery). This synthetic data validates
    the pipeline.
    """
    np.random.seed(7)
    X = np.zeros((n_samples, img_size, img_size, CHANNELS))
    y = np.zeros(n_samples, dtype=int)

    # Each identity: base shirt color + height range
    id_colors = np.random.randint(20, 235, (n_identities, 3))
    id_heights = np.random.uniform(0.4, 0.9, n_identities)  # fraction of frame

    for i in range(n_samples):
        identity = np.random.randint(0, n_identities)
        color = id_colors[identity]
        height_frac = id_heights[identity]
        # Background: porch/door
        bg = np.random.randint(80, 180, 3)
        X[i, :, :] = bg + np.random.normal(0, 10, (img_size, img_size, CHANNELS))
        # Person: vertical rectangle (torso) in shirt color
        h = int(img_size * height_frac)
        x0 = np.random.randint(img_size // 4, 3 * img_size // 4)
        w = int(img_size * np.random.uniform(0.15, 0.25))
        y0 = img_size - h
        X[i, y0:y0+h, x0:x0+w, :] = color
        X[i, y0:y0+h, x0:x0+w, :] += np.random.normal(0, 10, (h, w, CHANNELS))
        # Head: smaller circle on top
        head_r = max(4, w // 3)
        yy, xx = np.ogrid[:img_size, :img_size]
        head_cx, head_cy = x0 + w // 2, y0 - head_r
        head_mask = (xx - head_cx)**2 + (yy - head_cy)**2 <= head_r**2
        X[i, head_mask] = [200, 170, 140]  # skin tone
        X[i] = np.clip(X[i], 0, 255)
        y[i] = identity

    return X / 255.0, y


def train_model(model, X_train, y_train, X_test, y_test, epochs):
    print("\n--- Training Person Re-ID ---")
    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=epochs,
        batch_size=32,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=4),
        ]
    )
    loss, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"\nRe-ID — Test loss: {loss:.4f}, accuracy: {acc:.4f}")


def quantize_tflite(embed_model, X_train, output_path):
    """Convert the embedding model (classifier head stripped) to TFLite INT8."""
    def representative_data():
        for i in range(min(200, len(X_train))):
            yield [X_train[i:i+1].astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(embed_model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_data
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite = converter.convert()
    with open(output_path, 'wb') as f:
        f.write(tflite)
    print(f"TFLite INT8 saved: {output_path} ({len(tflite)/1024:.1f} KB)")


def evaluate_embedding_quality(embed_model, X_test, y_test, n_identities):
    """Verify that same-identity embeddings cluster (cosine similarity)."""
    embeddings = embed_model.predict(X_test, verbose=0)
    # Normalize
    embeddings = embeddings / (np.linalg.norm(embeddings, axis=1, keepdims=True) + 1e-8)
    # Sample pairs
    same_sim = []
    diff_sim = []
    n = min(500, len(X_test))
    idx = np.random.choice(len(X_test), n, replace=False)
    for i in range(n):
        for j in range(i + 1, n):
            sim = float(np.dot(embeddings[idx[i]], embeddings[idx[j]]))
            if y_test[idx[i]] == y_test[idx[j]]:
                same_sim.append(sim)
            else:
                diff_sim.append(sim)
    if same_sim and diff_sim:
        print(f"\nRe-ID embedding quality:")
        print(f"  Same-identity cosine sim:  mean={np.mean(same_sim):.3f} "
              f"(target >0.6)")
        print(f"  Diff-identity cosine sim:   mean={np.mean(diff_sim):.3f} "
              f"(target <0.4)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--identities", type=int, default=20)
    parser.add_argument("--samples", type=int, default=6000)
    parser.add_argument("--epochs", type=int, default=80)
    args = parser.parse_args()

    print("Generating person re-ID data...")
    X, y = generate_person_data(args.identities, args.samples)
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42)

    model, embed_model = build_reid_with_classifier(args.identities)
    train_model(model, X_tr, y_tr, X_te, y_te, args.epochs)

    evaluate_embedding_quality(embed_model, X_te, y_te, args.identities)
    quantize_tflite(embed_model, X_tr, "person_reid.tflite")


if __name__ == "__main__":
    main()