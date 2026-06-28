"""
PestSync ML Pipeline — Mobile Pest ID (MobileNetV3)
software/ml-pipeline/train_pest_id_mobile.py

Trains MobileNetV3-Small for on-device pest identification from
smartphone photos. 20 classes (15 pest + 5 "not a pest" categories).
Quantized for mobile inference.
"""
import os
import sys

# This script requires:
# pip install torch torchvision timm

import torch
import torch.nn as nn
from torch.utils.data import DataLoader
from torchvision import transforms, datasets

CLASSES = [
    "house_mouse", "norway_rat", "german_cockroach", "american_cockroach",
    "argentine_ant", "carpenter_ant", "mosquito", "house_fly",
    "fruit_fly", "bedbug", "termite_worker", "termite_swarmer",
    "spider", "silverfish", "carpet_beetle",
    "ladybug",  # not a pest (beneficial)
    "aphid",    # garden pest, not household
    "moth",     # common but usually harmless
    "cricket",  # nuisance but not harmful
    "centipede",  # beneficial predator
]

NUM_CLASSES = len(CLASSES)
IMAGE_SIZE = 224
BATCH_SIZE = 64
EPOCHS = 50
LEARNING_RATE = 0.001
DATA_DIR = "data/pest_id_images"
MODEL_OUTPUT = "models/pest_id_mobilenetv3.onnx"


def get_model():
    """Get MobileNetV3-Small with custom classifier head."""
    try:
        import timm
        model = timm.create_model("mobilenetv3_small_100", pretrained=True, num_classes=NUM_CLASSES)
    except ImportError:
        from torchvision.models import mobilenet_v3_small, MobileNet_V3_Small_Weights
        model = mobilenet_v3_small(weights=MobileNet_V3_Small_Weights.IMAGENET1K_V1)
        model.classifier[-1] = nn.Linear(model.classifier[-1].in_features, NUM_CLASSES)
    return model


def get_transforms():
    """Get train and validation transforms with augmentation."""
    train_transform = transforms.Compose([
        transforms.Resize((IMAGE_SIZE, IMAGE_SIZE)),
        transforms.RandomHorizontalFlip(),
        transforms.RandomRotation(15),
        transforms.ColorJitter(brightness=0.3, contrast=0.3, saturation=0.2, hue=0.1),
        transforms.RandomAffine(degrees=0, translate=(0.1, 0.1), scale=(0.9, 1.1)),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
    ])
    val_transform = transforms.Compose([
        transforms.Resize((IMAGE_SIZE, IMAGE_SIZE)),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
    ])
    return train_transform, val_transform


def train():
    """Train the MobileNetV3 pest ID model."""
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    train_tfm, val_tfm = get_transforms()

    train_dataset = datasets.ImageFolder(os.path.join(DATA_DIR, "train"), transform=train_tfm)
    val_dataset = datasets.ImageFolder(os.path.join(DATA_DIR, "val"), transform=val_tfm)

    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False, num_workers=4)

    model = get_model().to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)

    best_acc = 0.0

    for epoch in range(EPOCHS):
        # Train
        model.train()
        running_loss = 0.0
        for images, labels in train_loader:
            images, labels = images.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(images)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            running_loss += loss.item()
        scheduler.step()

        # Validate
        model.eval()
        correct = 0
        total = 0
        with torch.no_grad():
            for images, labels in val_loader:
                images, labels = images.to(device), labels.to(device)
                outputs = model(images)
                _, predicted = torch.max(outputs, 1)
                total += labels.size(0)
                correct += (predicted == labels).sum().item()

        val_acc = 100 * correct / total
        print(f"Epoch {epoch+1}/{EPOCHS} - Loss: {running_loss/len(train_loader):.4f} "
              f"- Val Acc: {val_acc:.2f}%")

        if val_acc > best_acc:
            best_acc = val_acc
            torch.save(model.state_dict(), "models/pest_id_mobilenetv3_best.pth")
            print(f"  → New best model saved ({val_acc:.2f}%)")

    print(f"\n✅ Best validation accuracy: {best_acc:.2f}%")

    # Export to ONNX for mobile
    export_to_onnx(model, device)


def export_to_onnx(model, device):
    """Export trained model to ONNX format for React Native mobile app."""
    model.eval()
    dummy_input = torch.randn(1, 3, IMAGE_SIZE, IMAGE_SIZE).to(device)
    os.makedirs("models", exist_ok=True)

    torch.onnx.export(
        model, dummy_input, MODEL_OUTPUT,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
    )
    size_mb = os.path.getsize(MODEL_OUTPUT) / (1024 * 1024)
    print(f"✅ ONNX model exported: {MODEL_OUTPUT} ({size_mb:.1f} MB)")

    # Also export quantized version
    try:
        import onnxruntime as ort
        from onnxruntime.quantization import quantize_dynamic, QuantType

        quantize_dynamic(
            MODEL_OUTPUT,
            MODEL_OUTPUT.replace(".onnx", "_int8.onnx"),
            weight_type=QuantType.QUInt8,
        )
        print(f"✅ Quantized model exported: {MODEL_OUTPUT.replace('.onnx', '_int8.onnx')}")
    except ImportError:
        print("⚠️ Install onnxruntime for quantization: pip install onnxruntime")


if __name__ == "__main__":
    if "--train" in sys.argv:
        train()
    else:
        print("Usage: python train_pest_id_mobile.py --train")