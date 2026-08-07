#!/usr/bin/env python3
"""
GuideSync — SceneNet (YOLOv8-nano) Training Script

Real-time object detection for blind navigation (80 classes).
Trains on COCO 2017 + custom blind-navigation dataset.

Output: TFLite-Micro int8 quantized model (~3.8 MB) for ESP32-S3.
"""
from __future__ import annotations

import os
import sys

import torch
import torchvision


def train_scenenet(data_dir: str = "data/coco", epochs: int = 50) -> None:
    """Train YOLOv8-nano for scene understanding."""
    try:
        from ultralytics import YOLO
    except ImportError:
        print("  [ERROR] ultralytics not installed. pip install ultralytics")
        return

    print("  Loading YOLOv8-nano pretrained weights...")
    model = YOLO("yolov8n.pt")

    # Priority mobility classes (COCO IDs):
    # person(0), chair(56), bed(59), couch(57), toilet(61),
    # bicycle(1), car(3), motorcycle(4), bus(5), truck(7),
    # traffic light(9), stop sign(11), bottle(39), cup(41),
    # laptop(63), cell phone(67), book(73), clock(74),
    # dog(16), cat(15)
    # Custom classes added via dataset config: white_cane, guide_dog,
    # trash_can, pole, wall, doorway, curb, puddle, overhanging_branch

    print(f"  Training on {data_dir} for {epochs} epochs...")
    results = model.train(
        data=os.path.join(data_dir, "guidesync.yaml"),
        epochs=epochs,
        imgsz=320,            # 320x320 for ESP32-S3 inference
        batch=32,
        device="0" if torch.cuda.is_available() else "cpu",
        patience=20,
        augment=True,
        mosaic=True,
        mixup=True,
        fliplr=True,
        hsv_h=0.015,
        hsv_s=0.7,
        hsv_v=0.4,
    )

    print(f"  Training complete. mAP@0.5: {results.results_dict.get('metrics/mAP50(B)', 'N/A')}")

    # Export to TFLite int8
    print("  Exporting to TFLite int8...")
    export_path = model.export(format="tflite", int8=True, imgsz=320)
    print(f"  Exported: {export_path}")

    # Report model size
    if os.path.exists(export_path):
        size_mb = os.path.getsize(export_path) / (1024 * 1024)
        print(f"  Model size: {size_mb:.1f} MB (target: <4 MB for ESP32-S3)")


if __name__ == "__main__":
    data = sys.argv[1] if len(sys.argv) > 1 else "data/coco"
    ep = int(sys.argv[2]) if len(sys.argv) > 2 else 50
    train_scenenet(data_dir=data, epochs=ep)