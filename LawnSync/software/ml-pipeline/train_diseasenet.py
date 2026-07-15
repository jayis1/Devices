"""
LawnSync ML Pipeline — DiseaseNet Training
MobileNetV3-Small backbone for 15-class lawn disease classification.

Dataset: 50,000 labeled lawn images (synthetic + real)
Classes: Healthy, Brown Patch, Dollar Spot, Rust, Fairy Ring, Snow Mold,
         Pythium Blight, Necrotic Ring Spot, Summer Patch, Powdery Mildew,
         Slime Mold, Dog Spot, Grub Damage, Chinch Bug, Sod Webworm

Output: int8 quantized TFLite model (~670 KB) for ESP32-S3 edge inference
"""

import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
from torchvision import transforms, datasets, models
from torch.quantization import quantize_dynamic

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DATA_DIR = os.environ.get("LAWNSYNC_DATA_DIR", "./data/disease_dataset")
MODEL_SAVE_DIR = "./models"
BATCH_SIZE = 64
NUM_EPOCHS = 50
LEARNING_RATE = 0.001
IMG_SIZE = 224
NUM_CLASSES = 15

DISEASE_CLASSES = [
    "Healthy", "Brown Patch", "Dollar Spot", "Rust", "Fairy Ring",
    "Snow Mold", "Pythium Blight", "Necrotic Ring Spot", "Summer Patch",
    "Powdery Mildew", "Slime Mold", "Dog Spot", "Grub Damage",
    "Chinch Bug", "Sod Webworm"
]

# ---------------------------------------------------------------------------
# Model: MobileNetV3-Small + Custom Head
# ---------------------------------------------------------------------------

class DiseaseNet(nn.Module):
    """MobileNetV3-Small backbone with custom classifier head for
    15-class lawn disease classification."""

    def __init__(self, num_classes: int = NUM_CLASSES, pretrained: bool = True):
        super().__init__()
        # Load MobileNetV3-Small backbone
        weights = models.MobileNet_V3_Small_Weights.IMAGENET1K_V1 if pretrained else None
        self.backbone = models.mobilenet_v3_small(weights=weights)
        # Remove original classifier
        in_features = self.backbone.classifier[-1].in_features  # 576
        self.backbone.classifier = nn.Sequential(
            nn.Linear(in_features, 256),
            nn.Hardswish(),
            nn.Dropout(0.3),
            nn.Linear(256, num_classes),
        )

    def forward(self, x):
        return self.backbone(x)

    def get_features(self, x):
        """Extract features before classifier (for embedding/transfer)."""
        x = self.backbone.features(x)
        x = self.backbone.avgpool(x)
        return torch.flatten(x, 1)


# ---------------------------------------------------------------------------
# Data Augmentation
# ---------------------------------------------------------------------------

train_transform = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.RandomHorizontalFlip(),
    transforms.RandomVerticalFlip(),
    transforms.RandomRotation(30),
    transforms.ColorJitter(brightness=0.2, contrast=0.2, saturation=0.2, hue=0.1),
    transforms.RandomAffine(degrees=0, translate=(0.1, 0.1)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
])

val_transform = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
])


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train_model():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[DiseaseNet] Training on {device}")

    # Data
    train_ds = datasets.ImageFolder(
        os.path.join(DATA_DIR, "train"), transform=train_transform)
    val_ds = datasets.ImageFolder(
        os.path.join(DATA_DIR, "val"), transform=val_transform)

    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False, num_workers=4)

    print(f"[DiseaseNet] Train: {len(train_ds)} images, Val: {len(val_ds)} images")
    print(f"[DiseaseNet] Classes: {train_ds.classes}")

    # Model
    model = DiseaseNet(num_classes=NUM_CLASSES, pretrained=True).to(device)

    # Class weights for imbalanced dataset
    class_counts = [0] * NUM_CLASSES
    for _, label in train_ds:
        class_counts[label] += 1
    weights = torch.tensor(class_counts, dtype=torch.float32)
    weights = (weights.sum() / weights).to(device)
    criterion = nn.CrossEntropyLoss(weight=weights)
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=NUM_EPOCHS)

    # Training loop
    best_val_acc = 0
    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)

    for epoch in range(NUM_EPOCHS):
        # Train
        model.train()
        train_loss = 0.0
        train_correct = 0
        for images, labels in train_loader:
            images, labels = images.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(images)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * images.size(0)
            train_correct += (outputs.argmax(1) == labels).sum().item()

        scheduler.step()
        train_acc = train_correct / len(train_ds)
        train_loss /= len(train_ds)

        # Validate
        model.eval()
        val_loss = 0.0
        val_correct = 0
        with torch.no_grad():
            for images, labels in val_loader:
                images, labels = images.to(device), labels.to(device)
                outputs = model(images)
                loss = criterion(outputs, labels)
                val_loss += loss.item() * images.size(0)
                val_correct += (outputs.argmax(1) == labels).sum().item()

        val_acc = val_correct / len(val_ds)
        val_loss /= len(val_ds)

        print(f"Epoch {epoch+1}/{NUM_EPOCHS} — "
              f"Train Loss: {train_loss:.4f}, Train Acc: {train_acc:.4f}, "
              f"Val Loss: {val_loss:.4f}, Val Acc: {val_acc:.4f}")

        # Save best model
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save(model.state_dict(),
                       os.path.join(MODEL_SAVE_DIR, "diseasenet_best.pth"))
            print(f"  → New best: {val_acc:.4f}")

    print(f"\n[DiseaseNet] Best validation accuracy: {best_val_acc:.4f}")

    # Quantize for edge deployment
    quantize_for_edge(model, device)

    return best_val_acc


def quantize_for_edge(model: nn.Module, device: torch.device):
    """Convert to ONNX + int8 quantized TFLite for ESP32-S3."""
    model.eval()
    model.to("cpu")

    # Export to ONNX
    dummy_input = torch.randn(1, 3, IMG_SIZE, IMG_SIZE)
    onnx_path = os.path.join(MODEL_SAVE_DIR, "diseasenet.onnx")
    torch.onnx.export(
        model, dummy_input, onnx_path,
        input_names=["input"], output_names=["output"],
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
        opset_version=13,
    )
    print(f"[DiseaseNet] Exported ONNX: {onnx_path}")

    # Dynamic quantization (reduces model size)
    quantized = quantize_dynamic(model, {nn.Linear}, dtype=torch.qint8)
    torch.save(quantized.state_dict(),
               os.path.join(MODEL_SAVE_DIR, "diseasenet_int8.pth"))
    print(f"[DiseaseNet] Quantized model saved")

    # In production: convert ONNX → TFLite with int8 calibration
    # Use onnx2tf or onnx-tf + tf.lite.TFLiteConverter with representative dataset
    # Output: diseasenet_quant.tflite (~670 KB)


if __name__ == "__main__":
    train_model()