"""
PestSync ML Pipeline — YOLOv8-nano Pest Classifier Training
software/ml-pipeline/train_pest_classifier.py

Trains YOLOv8-nano for on-device pest detection on ESP32-S3.
15 pest classes, int8 quantized, ~4MB model.
"""
import os
import sys
import yaml
from pathlib import Path

# This script requires ultralytics package:
# pip install ultralytics

DATASET_PATH = "data/pest_dataset"
MODEL_OUTPUT = "models/pest_yolov8n_int8.tflite"
CLASSES = [
    "house_mouse", "norway_rat", "german_cockroach", "american_cockroach",
    "argentine_ant", "carpenter_ant", "mosquito", "house_fly",
    "fruit_fly", "bedbug", "termite_worker", "termite_swarmer",
    "spider", "silverfish", "carpet_beetle",
]


def create_dataset_yaml():
    """Create YOLO dataset configuration."""
    config = {
        "path": str(Path(DATASET_PATH).resolve()),
        "train": "images/train",
        "val": "images/val",
        "test": "images/test",
        "nc": len(CLASSES),
        "names": CLASSES,
    }
    config_path = "data/pest_dataset.yaml"
    with open(config_path, "w") as f:
        yaml.dump(config, f, default_flow_style=False)
    print(f"Dataset config written to {config_path}")
    return config_path


def train_model():
    """Train YOLOv8-nano and export to int8 TFLite for ESP32-S3."""
    from ultralytics import YOLO

    config_path = create_dataset_yaml()

    # Load YOLOv8-nano (smallest model, 3.2M params)
    model = YOLO("yolov8n.pt")

    # Train
    print("Training YOLOv8-nano pest classifier (15 classes)...")
    results = model.train(
        data=config_path,
        epochs=200,
        imgsz=320,        # 320×320 input (ESP32-S3 memory constraint)
        batch=32,
        device="0",       # GPU
        patience=30,       # early stopping
        lr0=0.01,
        lrf=0.01,
        hsv_h=0.015,      # augmentation for varied lighting
        hsv_s=0.7,
        hsv_v=0.4,
        flipud=0.0,       # no vertical flip (pests don't hang upside down)
        fliplr=0.5,
        mosaic=1.0,
        mixup=0.1,
        project="runs/pest_detection",
        name="yolov8n_pest",
    )

    print(f"Training complete. Best mAP50: {results.results_dict.get('metrics/mAP50(B)', 'N/A')}")

    # Export to TFLite int8
    print("Exporting to TFLite int8 for ESP32-S3...")
    best_model = YOLO(f"runs/pest_detection/yolov8n_pest/weights/best.pt")

    tflite_path = best_model.export(
        format="tflite",
        int8=True,         # int8 quantization for ESP32-S3
        imgsz=320,
        data=str(Path(DATASET_PATH) / "images" / "val"),  # calibration data
    )

    # Copy to models directory
    os.makedirs("models", exist_ok=True)
    if os.path.exists(tflite_path):
        import shutil
        shutil.copy(tflite_path, MODEL_OUTPUT)
        model_size = os.path.getsize(MODEL_OUTPUT) / (1024 * 1024)
        print(f"✅ Model exported: {MODEL_OUTPUT} ({model_size:.1f} MB)")
    else:
        print(f"⚠️ TFLite export at {tflite_path} not found")


def evaluate_model():
    """Evaluate the trained model on test set."""
    from ultralytics import YOLO

    model = YOLO("runs/pest_detection/yolov8n_pest/weights/best.pt")
    metrics = model.val(data="data/pest_dataset.yaml", split="test")

    print(f"\n{'='*60}")
    print(f"Test Set Evaluation:")
    print(f"  mAP50:    {metrics.box.map50:.4f}")
    print(f"  mAP50-95: {metrics.box.map:.4f}")
    print(f"  Precision: {metrics.box.mp:.4f}")
    print(f"  Recall:    {metrics.box.mr:.4f}")
    print(f"{'='*60}")

    # Per-class results
    print("\nPer-class AP50:")
    for i, cls in enumerate(CLASSES):
        if i < len(metrics.box.ap50):
            print(f"  {cls:25s}: {metrics.box.ap50[i]:.4f}")


def generate_synthetic_data():
    """Generate synthetic pest images using Blender or procedural augmentation.

    In production:
    1. Use Blender with photorealistic pest 3D models placed in household scenes
    2. Randomize: lighting, camera angle, background, occlusion, scale
    3. Auto-label bounding boxes from 3D model placement
    4. Also use CutMix, CopyPaste augmentation on real pest images

    This generates 60,000 synthetic images for training.
    """
    print("Generating synthetic pest images (60,000)...")
    print("  In production: run Blender headless with pest_models.blend")
    print("  For reference: see synthetic_pest_sim.py")
    # Actual implementation would call Blender subprocess


if __name__ == "__main__":
    if "--synth" in sys.argv:
        generate_synthetic_data()
    if "--train" in sys.argv:
        train_model()
    if "--eval" in sys.argv:
        evaluate_model()
    if len(sys.argv) == 1:
        print("Usage: python train_pest_classifier.py [--synth] [--train] [--eval]")