"""
GreenPulse Pest Detector Training — YOLOv8-nano

Trains an object-detection model for common houseplant pests:
spider mites, aphids, mealybugs, thrips, whiteflies, fungus gnats, scale.

Detects and counts pests per leaf; severity scoring based on count + density.
Runs as cloud inference (leaf scanner uploads multispectral images).

Uses Ultralytics YOLOv8-nano for real-time detection on uploaded images.
"""

import os

# In production: from ultralytics import YOLO
# This script requires ultralytics package: pip install ultralytics

PEST_CLASSES = [
    "spider_mite", "aphid", "mealybug", "thrips", "whitefly",
    "fungus_gnat", "scale_insect",
]

DATASET_YAML = """
path: ./data/pests
train: images/train
val: images/val
names:
  0: spider_mite
  1: aphid
  2: mealybug
  3: thrips
  4: whitefly
  5: fungus_gnat
  6: scale_insect
"""


def train():
    """Train YOLOv8-nano on pest detection dataset.

    Dataset: IP102 (pest subset) + augmented synthetic overlays on leaf
    backgrounds + proprietary captures from leaf scanner multispectral mode.
    """
    yaml_path = os.path.join(os.path.dirname(__file__), "pest_dataset.yaml")
    with open(yaml_path, "w") as f:
        f.write(DATASET_YAML)

    # In production:
    # from ultralytics import YOLO
    # model = YOLO("yolov8n.pt")  # nano for speed
    # results = model.train(
    #     data=yaml_path,
    #     epochs=100,
    #     imgsz=640,
    #     batch=32,
    #     device="cuda" if available else "cpu",
    # )
    # model.export(format="onnx")
    print("Pest detector training script.")
    print("Requires: pip install ultralytics")
    print(f"Dataset config: {yaml_path}")
    print("In production: trains YOLOv8-nano on IP102 + synthetic pest overlays")
    print("Export: ONNX for cloud inference, TFLite for edge (optional)")


def count_severity(detections):
    """Compute pest severity from detection results.

    Severity scale:
      0-5 pests:   low (monitor)
      6-20 pests:  medium (treat soon)
      21+ pests:   high (treat immediately)
    """
    total = sum(len(d) for d in detections)
    if total <= 5:
        return "low", total
    elif total <= 20:
        return "medium", total
    else:
        return "high", total


if __name__ == "__main__":
    train()