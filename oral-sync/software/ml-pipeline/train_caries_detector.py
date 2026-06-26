"""
Model 4: Caries white-spot detector
  Input:  405 nm + NIR difference image 416×416 (demineralization signature)
  Output: lesion bounding boxes + probability (YOLOv8-nano)
  Export: ONNX for cloud inference service
"""
from ultralytics import YOLO

def main():
    # YOLOv8-nano — smallest variant, runs fast in cloud
    model = YOLO("yolov8n.pt")
    # data/caries/data.yaml defines train/val paths + 1 class: "white_spot_lesion"
    results = model.train(
        data="data/caries/data.yaml",
        epochs=100,
        imgsz=416,
        batch=16,
        name="caries_yolov8n",
        project="artifacts",
    )
    # Export to ONNX
    path = model.export(format="onnx", imgsz=416)
    print(f"✓ Exported ONNX: {path}")

if __name__ == "__main__":
    main()