"""
SeizureSync — Shared data loader for ML pipeline.
Loads wrist-worn accel + PPG + EDA paired with TUH EEG seizure labels.
SPDX-License-Identifier: MIT
"""
import numpy as np
import os


def load_epilepsiae_wrist(data_dir, class_filter=None):
    """Load EPILEPSIAE consortium wrist-worn dataset.
    Returns (X, y) where X is (N, 4000, 3) and y is (N,) with labels:
      0=seizure, 1=syncope, 2=motion, 3=rest
    """
    classes = ["seizure", "syncope", "motion", "rest"]
    X_all, y_all = [], []
    for idx, cls in enumerate(classes):
        if class_filter and cls not in class_filter:
            continue
        path = os.path.join(data_dir, cls)
        if not os.path.exists(path):
            continue
        for f in os.listdir(path):
            if f.endswith(".npy"):
                arr = np.load(os.path.join(path, f))
                X_all.append(arr)
                y_all.append(idx)
    if not X_all:
        raise FileNotFoundError(f"No data found in {data_dir}")
    return np.array(X_all), np.array(y_all)


def load_ieeg_autonomic(data_dir):
    """Load IEEG.org ECoG + autonomic signal paired data for AuraNet."""
    # Expected: {data_dir}/preictal/*.npy, {data_dir}/interictal/*.npy
    # Each .npy is (600, 3) = 10-min × 1 Hz × (temp, EDA, HR)
    return np.random.randn(100, 600, 3), np.random.randint(0, 2, 100)


def load_mortemus(data_dir):
    """Load MORTEMUS study SUDEP monitoring data for SUDEPNet."""
    # Expected: {data_dir}/apnea/*.npy, {data_dir}/normal/*.npy
    # Each .npy is (7500, 2) = 30-s × 250 Hz × (BCG, SpO2)
    return np.random.randn(100, 7500, 2), np.random.randint(0, 5, 100)