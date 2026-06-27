"""
Add-Item Classifier Training
Trains a MobileNetV3-Small model to classify whether items are compostable.

50 categories:
  compostable_greens: vegetable scraps, fruit scraps, coffee grounds, tea bags,
                     grass clippings, eggshells, manure, flowers
  compostable_browns: cardboard, paper, dry leaves, sawdust, twigs, straw, napkins
  not_compostable:   plastic, metal, glass, styrofoam, batteries, diapers,
                     meat, bones, dairy, oils, diseased plants
  recycle:           aluminum can, plastic bottle, paper, cardboard box
  trash:             cigarette butts, gum, synthetic fabric
"""
import os
import logging
import torch
import torch.nn as nn
from torch.utils.data import DataLoader
from torchvision import datasets, transforms, models
from torch.optim import Adam

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

NUM_CLASSES = 50
BATCH_SIZE = 64
EPOCHS = 30
LR = 0.001
IMAGE_SIZE = 224
DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")


def get_model():
    """Load MobileNetV3-Small with custom head."""
    model = models.mobilenet_v3_small(weights=models.MobileNet_V3_Small_Weights.DEFAULT)
    model.classifier[3] = nn.Linear(model.classifier[3].in_features, NUM_CLASSES)
    return model


def train():
    """Train the add-item classifier."""
    train_transform = transforms.Compose([
        transforms.Resize((IMAGE_SIZE, IMAGE_SIZE)),
        transforms.RandomHorizontalFlip(),
        transforms.RandomRotation(15),
        transforms.ColorJitter(brightness=0.2, contrast=0.2, saturation=0.2),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
    ])

    val_transform = transforms.Compose([
        transforms.Resize((IMAGE_SIZE, IMAGE_SIZE)),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
    ])

    train_ds = datasets.ImageFolder("data/images/train", transform=train_transform)
    val_ds = datasets.ImageFolder("data/images/val", transform=val_transform)
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, num_workers=4)

    logger.info(f"Train: {len(train_ds)} images, Val: {len(val_ds)} images")
    logger.info(f"Classes: {train_ds.classes}")

    model = get_model().to(DEVICE)
    criterion = nn.CrossEntropyLoss()
    optimizer = Adam(model.parameters(), lr=LR)
    scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=10, gamma=0.5)

    best_acc = 0.0
    for epoch in range(EPOCHS):
        model.train()
        running_loss = 0
        for images, labels in train_loader:
            images, labels = images.to(DEVICE), labels.to(DEVICE)
            optimizer.zero_grad()
            outputs = model(images)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            running_loss += loss.item()

        # Validation
        model.eval()
        correct = 0
        total = 0
        with torch.no_grad():
            for images, labels in val_loader:
                images, labels = images.to(DEVICE), labels.to(DEVICE)
                outputs = model(images)
                _, predicted = outputs.max(1)
                total += labels.size(0)
                correct += (predicted == labels).sum().item()

        acc = correct / total
        logger.info(f"Epoch {epoch+1}/{EPOCHS} — loss={running_loss/len(train_loader):.4f} val_acc={acc:.4f}")

        if acc > best_acc:
            best_acc = acc
            torch.save(model.state_dict(), "models/additem_mobilenetv3.pt")
            logger.info(f"  → Saved best model (acc={acc:.4f})")

        scheduler.step()

    # Export quantized model for mobile deployment
    model.load_state_dict(torch.load("models/additem_mobilenetv3.pt"))
    model.eval()
    model_quantized = torch.quantization.quantize_dynamic(
        model, {nn.Linear}, dtype=torch.qint8
    )
    torch.jit.save(torch.jit.script(model_quantized), "models/additem_quantized.pt")
    logger.info("Exported quantized model: models/additem_quantized.pt")


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train()