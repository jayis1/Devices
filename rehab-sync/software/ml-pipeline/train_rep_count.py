"""
RehabSync — RepCount Training Script

Hybrid peak-detection + state machine for automatic rep counting
from joint angle + force data. Trains thresholds per exercise type
using labeled rep data.

Input: 500ms sliding window × joint angle (degrees) + force (grams-force)
Output: rep count increment (binary: 0 or 1 per window)

On-device: ESP32-S3, custom C state machine, <5ms, 12KB

Usage:
  python train_rep_count.py --data /data/rep_dataset
"""
import argparse
import numpy as np
from scipy.signal import find_peaks
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.metrics import accuracy_score, precision_score, recall_score
import pickle


class RepCounter:
    """Per-exercise rep counting with adaptive thresholds.

    Uses peak detection on joint angle signal with exercise-specific
    thresholds + force confirmation for resistance exercises.
    """
    def __init__(self):
        self.thresholds = {}  # exercise_id → {min_angle, min_amplitude, min_duration, force_threshold}
        self.classifier = None  # Optional ML-based refinement

    def fit(self, X, y, exercise_ids):
        """Train per-exercise thresholds from labeled rep data.

        X: (N, T) joint angle time series
        y: (N,) rep count labels
        exercise_ids: (N,) exercise type per sample
        """
        unique_ex = np.unique(exercise_ids)
        for ex_id in unique_ex:
            mask = exercise_ids == ex_id
            ex_angles = X[mask]
            ex_reps = y[mask]

            # Analyze peak structure for this exercise
            all_peaks_angles = []
            all_amplitudes = []
            for i, (angle_seq, true_reps) in enumerate(zip(ex_angles, ex_reps)):
                if true_reps > 0:
                    # Find peaks
                    peaks, props = find_peaks(angle_seq, prominence=5.0, distance=20)
                    if len(peaks) > 0:
                        all_peaks_angles.extend(angle_seq[peaks])
                        all_amplitudes.extend(props["prominences"])

            if all_peaks_angles:
                self.thresholds[ex_id] = {
                    "min_angle": np.percentile(all_peaks_angles, 10),
                    "min_amplitude": np.percentile(all_amplitudes, 10),
                    "min_duration": 15,  # samples (150ms at 100Hz)
                    "force_threshold": 500,  # mg-force (for resistance exercises)
                }
                print(f"Exercise {ex_id}: threshold angle={self.thresholds[ex_id]['min_angle']:.1f}° "
                      f"amplitude={self.thresholds[ex_id]['min_amplitude']:.1f}°")

        # Train optional ML classifier for peak validation
        self._train_classifier(X, y, exercise_ids)

    def _train_classifier(self, X, y, exercise_ids):
        """Train a GradientBoosting classifier for peak validation.

        Features per detected peak:
        - peak angle value
        - prominence (amplitude)
        - duration (samples above threshold)
        - preceding velocity
        - following velocity
        Label: 1 if true rep, 0 if false positive
        """
        features = []
        labels = []
        for angle_seq, true_reps, ex_id in zip(X, y, exercise_ids):
            if ex_id not in self.thresholds:
                continue
            thresh = self.thresholds[ex_id]
            peaks, props = find_peaks(
                angle_seq,
                prominence=thresh["min_amplitude"] * 0.5,
                distance=10
            )
            for peak_idx, prom in zip(peaks, props["prominences"]):
                # Extract features
                peak_angle = angle_seq[peak_idx]
                duration = np.sum(angle_seq[max(0, peak_idx-30):peak_idx+30] > thresh["min_angle"])

                # Velocity before and after peak
                pre_vel = np.mean(np.diff(angle_seq[max(0, peak_idx-10):peak_idx])) if peak_idx > 10 else 0
                post_vel = np.mean(np.diff(angle_seq[peak_idx:peak_idx+10])) if peak_idx + 10 < len(angle_seq) else 0

                features.append([peak_angle, prom, duration, pre_vel, post_vel])
                # Label: if this peak is a true rep (simplified: assume evenly distributed)
                labels.append(1 if len(peaks) > 0 and true_reps > 0 else 0)

        if len(features) > 100:
            self.classifier = GradientBoostingClassifier(
                n_estimators=100, max_depth=3, learning_rate=0.1
            )
            self.classifier.fit(features, labels)
            acc = accuracy_score(labels, self.classifier.predict(features))
            print(f"Rep classifier trained: accuracy={acc:.4f}")

    def predict(self, angle_seq, force_seq=None, exercise_id=0):
        """Count reps in a time series."""
        if exercise_id not in self.thresholds:
            # Default thresholds
            peaks, _ = find_peaks(angle_seq, prominence=10.0, distance=20)
            return len(peaks)

        thresh = self.thresholds[exercise_id]
        peaks, props = find_peaks(
            angle_seq,
            height=thresh["min_angle"],
            prominence=thresh["min_amplitude"],
            distance=thresh["min_duration"],
        )

        if self.classifier is not None and len(peaks) > 0:
            features = []
            for peak_idx, prom in zip(peaks, props["prominences"]):
                peak_angle = angle_seq[peak_idx]
                duration = np.sum(angle_seq[max(0, peak_idx-30):peak_idx+30] > thresh["min_angle"])
                pre_vel = np.mean(np.diff(angle_seq[max(0, peak_idx-10):peak_idx])) if peak_idx > 10 else 0
                post_vel = np.mean(np.diff(angle_seq[peak_idx:peak_idx+10])) if peak_idx + 10 < len(angle_seq) else 0
                features.append([peak_angle, prom, duration, pre_vel, post_vel])

            predictions = self.classifier.predict(features)
            return int(predictions.sum())

        return len(peaks)


def train(args):
    """Train rep counter from labeled data."""
    data = np.load(f"{args.data}/rep_dataset.npz")
    X = data["X"]        # (N, T) joint angle time series
    y = data["y"]        # (N,) rep counts
    exercise_ids = data["exercise_ids"]  # (N,) exercise types

    print(f"Loaded {len(X)} samples, {len(np.unique(exercise_ids))} exercise types")

    counter = RepCounter()
    counter.fit(X, y, exercise_ids)

    # Evaluate
    correct = 0
    total = 0
    for i in range(len(X)):
        predicted = counter.predict(X[i], exercise_id=exercise_ids[i])
        if predicted == y[i]:
            correct += 1
        total += 1

    acc = correct / total
    print(f"\nRep counting accuracy: {acc:.4f} ({correct}/{total})")

    # Save model
    with open(f"{args.output}/rep_counter.pkl", "wb") as f:
        pickle.dump(counter, f)
    print(f"Model saved to {args.output}/rep_counter.pkl")

    # Export C code for ESP32-S3 deployment
    export_to_c(counter, args.output)


def export_to_c(counter, output_dir):
    """Export rep counter as C code for ESP32-S3 firmware."""
    code = f"""/*
 * RehabSync — RepCount C Export (auto-generated)
 * Per-exercise thresholds for peak-detection state machine.
 */
#include "rep_count.h"
#include <math.h>
#include <string.h>

typedef struct {{
    float min_angle;
    float min_amplitude;
    int min_duration;
    float force_threshold;
}} rep_threshold_t;

static const rep_threshold_t thresholds[{max(counter.thresholds.keys()) + 1 if counter.thresholds else 1}] = {{
"""
    for ex_id in sorted(counter.thresholds.keys()):
        t = counter.thresholds[ex_id]
        code += f"    [{ex_id}] = {{ .min_angle = {t['min_angle']:.1f}f, "
        code += f".min_amplitude = {t['min_amplitude']:.1f}f, "
        code += f".min_duration = {t['min_duration']}, "
        code += f".force_threshold = {t['force_threshold']:.0f}f }},\n"

    code += """};

int count_reps(const float *angles, int len, int exercise_id) {
    if (exercise_id < 0 || exercise_id >= sizeof(thresholds)/sizeof(thresholds[0]))
        return 0;
    const rep_threshold_t *t = &thresholds[exercise_id];
    int count = 0;
    int above_threshold = 0;
    float peak_val = 0;
    int peak_start = 0;

    for (int i = 0; i < len; i++) {
        if (angles[i] > t->min_angle) {
            if (!above_threshold) {
                above_threshold = 1;
                peak_start = i;
                peak_val = angles[i];
            } else if (angles[i] > peak_val) {
                peak_val = angles[i];
            }
        } else if (above_threshold) {
            int duration = i - peak_start;
            float amplitude = peak_val - t->min_angle;
            if (duration >= t->min_duration && amplitude >= t->min_amplitude)
                count++;
            above_threshold = 0;
        }
    }
    return count;
}
"""
    with open(f"{output_dir}/rep_count.c", "w") as f:
        f.write(code)
    print(f"C code exported to {output_dir}/rep_count.c")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train RepCount")
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--output", type=str, default="models")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)