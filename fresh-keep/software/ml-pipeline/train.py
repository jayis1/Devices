"""
FreshKeep — ML Pipeline: Food Detection Model Training

Trains an EfficientDet-D0 model for detecting and classifying food items
in fridge/pantry camera images. Outputs a TFLite INT8 model for
on-hub inference (MobileNet V2 SSD) and a PyTorch model for cloud inference.

Training data: Custom kitchen food dataset + Food-101 + Grocery Store Dataset
"""

import os
import argparse
import json
from pathlib import Path

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torchvision import transforms, models
from torchvision.models.detection import fasterrcnn_mobilenet_v3_large_fpn
from torchvision.models.detection.faster_rcnn import FastRCNNPredictor

# ── Food Categories ───────────────────────────────────────────────────────
FOOD_CATEGORIES = [
    # Fresh produce
    "apple", "banana", "orange", "strawberry", "blueberry",
    "tomato", "avocado", "lemon", "lime", "grape",
    "carrot", "broccoli", "spinach", "lettuce", "bell_pepper",
    "onion", "garlic", "potato", "sweet_potato", "mushroom",
    # Dairy
    "milk", "cheese", "yogurt", "butter", "cream",
    "egg", "cottage_cheese", "sour_cream", "cream_cheese", "whipping_cream",
    # Meat & protein
    "chicken_breast", "ground_beef", "steak", "bacon", "sausage",
    "fish_fillet", "shrimp", "tofu", "deli_meat", "salmon",
    # Grains & bread
    "bread", "rice", "pasta", "cereal", "flour",
    "tortilla", "cracker", "oatmeal", "quinoa", "noodle",
    # Condiments & sauces
    "ketchup", "mustard", "mayo", "soy_sauce", "hot_sauce",
    "salad_dressing", "olive_oil", "vinegar", "jam", "honey",
    # Beverages
    "juice", "soda", "water_bottle", "beer", "wine",
    "coffee_creamer", "tea", "sports_drink", "milk_alternative", "sparkling_water",
    # Leftovers & prepared
    "leftover_container", "meal_prep", "takeout_box", "soup_container", "salad_bowl",
    # Packaging
    "egg_carton", "milk_carton", "jar", "can", "bottle",
]

NUM_CLASSES = len(FOOD_CATEGORIES) + 1  # +1 for background


# ── Dataset ────────────────────────────────────────────────────────────────
class KitchenFoodDataset(Dataset):
    """Custom dataset for fridge/pantry food detection.
    
    Expected directory structure:
        data/
        ├── train/
        │   ├── images/
        │   │   ├── 0001.jpg
        │   │   ├── 0002.jpg
        │   │   └── ...
        │   └── labels/
        │       ├── 0001.json
        │       ├── 0002.json
        │       └── ...
        ├── val/
        │   ├── images/
        │   └── labels/
        └── test/
            ├── images/
            └── labels/
    
    Label JSON format:
    {
        "boxes": [[x1, y1, x2, y2], ...],  # Normalized 0-1
        "labels": [0, 1, ...],                # Category indices
        "spoiled": [false, true, ...]          # Whether item appears spoiled
    }
    """
    
    def __init__(self, root: str, split: str = "train", transform=None):
        self.root = Path(root) / split
        self.transform = transform
        self.images = sorted(list((self.root / "images").glob("*.jpg")))
        
    def __len__(self):
        return len(self.images)
    
    def __getitem__(self, idx):
        from PIL import Image
        
        img_path = self.images[idx]
        label_path = self.root / "labels" / f"{img_path.stem}.json"
        
        image = Image.open(img_path).convert("RGB")
        
        with open(label_path) as f:
            label_data = json.load(f)
        
        boxes = torch.tensor(label_data["boxes"], dtype=torch.float32)
        labels = torch.tensor(label_data["labels"], dtype=torch.int64)
        spoiled = torch.tensor(label_data.get("spoiled", [False]*len(labels)), dtype=torch.bool)
        
        target = {
            "boxes": boxes,
            "labels": labels,
            "spoiled": spoiled,
            "image_id": torch.tensor([idx]),
        }
        
        if self.transform:
            image = self.transform(image)
        
        return image, target


# ── Model ──────────────────────────────────────────────────────────────────
def create_food_detection_model(num_classes: int):
    """Create EfficientDet-style food detection model based on MobileNet V3."""
    # Use Faster R-CNN with MobileNet V3 backbone (good balance of speed/accuracy)
    model = fasterrcnn_mobilenet_v3_large_fpn(
        pretrained=True,
        pretrained_backbone=True,
    )
    
    # Replace the classification head with our number of classes
    in_features = model.roi_heads.box_predictor.cls_score.in_features
    model.roi_heads.box_predictor = FastRCNNPredictor(in_features, num_classes)
    
    return model


class SpoilageClassifier(nn.Module):
    """Lightweight classifier that predicts spoilage score from sensor data.
    
    Input: [voc_index, co2_ppm, ethylene_raw, temp_c, humidity, days_since_purchase]
    Output: spoilage_score (0-100)
    """
    
    def __init__(self):
        super().__init__()
        self.conv1 = nn.Conv1d(1, 32, kernel_size=3, padding=1)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=3, padding=1)
        self.conv3 = nn.Conv1d(64, 128, kernel_size=3, padding=1)
        self.gru = nn.GRU(128, 64, num_layers=2, batch_first=True)
        self.fc1 = nn.Linear(64 + 6, 128)  # GRU output + current features
        self.fc2 = nn.Linear(128, 64)
        self.fc3 = nn.Linear(64, 1)  # Spoilage score 0-100
        
    def forward(self, time_series, current_features):
        """
        Args:
            time_series: (batch, seq_len, 6) — historical sensor readings
            current_features: (batch, 6) — current sensor readings
        """
        x = time_series.transpose(1, 2)  # (batch, 6, seq_len)
        x = torch.relu(self.conv1(x))
        x = torch.relu(self.conv2(x))
        x = torch.relu(self.conv3(x))
        x = x.transpose(1, 2)  # (batch, seq_len, 128)
        
        _, h = self.gru(x)  # h: (2, batch, 64)
        h = h[-1]  # Take last layer: (batch, 64)
        
        combined = torch.cat([h, current_features], dim=1)  # (batch, 70)
        x = torch.relu(self.fc1(combined))
        x = torch.relu(self.fc2(x))
        x = torch.sigmoid(self.fc3(x)) * 100  # Scale to 0-100
        
        return x.squeeze(-1)


class FireDetectionTiny(nn.Module):
    """Tiny MobileNet V1 classifier for fire vs. cooking thermal signatures.
    
    Designed to run on STM32F411 (Cortex-M4) with TFLite Micro.
    Input: 32×24 thermal frame + 3 gas readings + 5-frame history
    Output: Fire confidence 0-1
    """
    
    def __init__(self):
        super().__init__()
        # Thermal frame processing (32×24 = 768 pixels, treated as 1-channel image)
        self.features = nn.Sequential(
            nn.Conv2d(1, 8, kernel_size=3, stride=2, padding=1),  # 16×12
            nn.ReLU(),
            nn.Conv2d(8, 16, kernel_size=3, stride=2, padding=1),  # 8×6
            nn.ReLU(),
            nn.Conv2d(16, 32, kernel_size=3, stride=2, padding=1),  # 4×3
            nn.ReLU(),
            nn.AdaptiveAvgPool2d((1, 1)),
        )
        # Gas + temporal features
        self.gas_fc = nn.Sequential(
            nn.Linear(8, 16),  # 3 gas readings + 5-frame history = 8
            nn.ReLU(),
        )
        # Combined classifier
        self.classifier = nn.Sequential(
            nn.Linear(32 + 16, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
            nn.Sigmoid(),
        )
    
    def forward(self, thermal_frame, gas_features):
        """
        Args:
            thermal_frame: (batch, 1, 24, 32) — normalized thermal data
            gas_features: (batch, 8) — [lpg, co, nh3, flame, smoke, hist1, hist2, hist3]
        """
        x = self.features(thermal_frame)
        x = x.view(x.size(0), -1)  # (batch, 32)
        
        g = self.gas_fc(gas_features)  # (batch, 16)
        
        combined = torch.cat([x, g], dim=1)
        return self.classifier(combined).squeeze(-1)


# ── Training Functions ─────────────────────────────────────────────────────
def collate_fn(batch):
    """Custom collate for variable-size bounding boxes."""
    return tuple(zip(*batch))


def train_food_detection(args):
    """Train food detection model."""
    print(f"Training food detection model with {NUM_CLASSES} classes")
    print(f"Categories: {FOOD_CATEGORIES}")
    
    # Data transforms
    train_transform = transforms.Compose([
        transforms.Resize((320, 320)),
        transforms.RandomHorizontalFlip(),
        transforms.ColorJitter(brightness=0.2, contrast=0.2, saturation=0.2),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
    ])
    
    val_transform = transforms.Compose([
        transforms.Resize((320, 320)),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
    ])
    
    # Datasets
    train_dataset = KitchenFoodDataset(args.data_dir, "train", train_transform)
    val_dataset = KitchenFoodDataset(args.data_dir, "val", val_transform)
    
    train_loader = DataLoader(
        train_dataset, batch_size=args.batch_size, shuffle=True,
        num_workers=4, collate_fn=collate_fn
    )
    val_loader = DataLoader(
        val_dataset, batch_size=1, shuffle=False,
        num_workers=4, collate_fn=collate_fn
    )
    
    # Model
    model = create_food_detection_model(NUM_CLASSES)
    model = model.to(args.device)
    
    # Optimizer
    params = [p for p in model.parameters() if p.requires_grad]
    optimizer = optim.SGD(params, lr=args.lr, momentum=0.9, weight_decay=0.0005)
    scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=3, gamma=0.1)
    
    # Training loop
    best_map = 0.0
    for epoch in range(args.epochs):
        model.train()
        epoch_loss = 0.0
        
        for i, (images, targets) in enumerate(train_loader):
            images = [img.to(args.device) for img in images]
            targets = [{k: v.to(args.device) for k, v in t.items()} for t in targets]
            
            optimizer.zero_grad()
            loss_dict = model(images, targets)
            losses = sum(loss for loss in loss_dict.values())
            losses.backward()
            optimizer.step()
            
            epoch_loss += losses.item()
            
            if (i + 1) % 10 == 0:
                print(f"Epoch {epoch+1}/{args.epochs}, "
                      f"Batch {i+1}/{len(train_loader)}, "
                      f"Loss: {losses.item():.4f}")
        
        scheduler.step()
        avg_loss = epoch_loss / len(train_loader)
        print(f"Epoch {epoch+1} complete. Average loss: {avg_loss:.4f}")
        
        # Save checkpoint
        checkpoint_path = Path(args.output_dir) / f"food_det_epoch_{epoch+1}.pth"
        torch.save({
            'epoch': epoch + 1,
            'model_state_dict': model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'loss': avg_loss,
        }, checkpoint_path)
    
    # Save final model
    final_path = Path(args.output_dir) / "food_detection_final.pth"
    torch.save(model.state_dict(), final_path)
    print(f"Model saved to {final_path}")


def train_spoilage(args):
    """Train spoilage prediction model."""
    print("Training spoilage prediction model (1D-CNN + GRU)")
    
    model = SpoilageClassifier().to(args.device)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    criterion = nn.MSELoss()
    
    # In production: load real sensor time-series data
    # For training script: generate synthetic data
    print("Note: Using synthetic training data. Replace with real sensor data for production.")
    
    # Synthetic data generation for demonstration
    num_samples = 10000
    seq_len = 60  # 60 readings (1 hour at 1/min)
    num_features = 6
    
    X_series = torch.randn(num_samples, seq_len, num_features) * torch.tensor([
        [100],    # VOC index
        [500],    # CO2 ppm deviation
        [100],    # Ethylene
        [5],      # Temperature
        [20],     # Humidity
        [7],      # Days since purchase
    ])
    
    X_current = torch.randn(num_samples, num_features) * torch.tensor([
        100, 500, 100, 5, 20, 7
    ])
    
    # Spoilage score: higher VOC, CO2, temp, ethylene, days → higher score
    y = (
        X_current[:, 0] / 500 * 25 +   # VOC contribution
        X_current[:, 1] / 2000 * 25 +   # CO2 contribution
        X_current[:, 2] / 1000 * 25 +   # Ethylene contribution
        X_current[:, 3] / 40 * 15 +     # Temperature contribution
        X_current[:, 5] / 14 * 10       # Days contribution
    )
    y = torch.clamp(y, 0, 100)
    
    # Split train/val
    split = int(num_samples * 0.8)
    train_series, val_series = X_series[:split], X_series[split:]
    train_current, val_current = X_current[:split], X_current[split:]
    train_y, val_y = y[:split], y[split:]
    
    # Training loop
    for epoch in range(args.epochs):
        model.train()
        total_loss = 0.0
        
        for i in range(0, split, args.batch_size):
            batch_series = train_series[i:i+args.batch_size].to(args.device)
            batch_current = train_current[i:i+args.batch_size].to(args.device)
            batch_y = train_y[i:i+args.batch_size].to(args.device)
            
            optimizer.zero_grad()
            pred = model(batch_series, batch_current)
            loss = criterion(pred, batch_y)
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item()
        
        avg_loss = total_loss / (split // args.batch_size)
        
        # Validation
        model.eval()
        with torch.no_grad():
            val_pred = model(val_series.to(args.device), val_current.to(args.device))
            val_loss = criterion(val_pred, val_y.to(args.device)).item()
        
        print(f"Epoch {epoch+1}/{args.epochs} — Train Loss: {avg_loss:.4f}, Val Loss: {val_loss:.4f}")
    
    # Save model
    model_path = Path(args.output_dir) / "spoilage_predictor.pth"
    torch.save(model.state_dict(), model_path)
    print(f"Spoilage model saved to {model_path}")


def train_fire_detection(args):
    """Train tiny fire detection model for stove guard MCU."""
    print("Training tiny fire detection model (MobileNet V1 + gas features)")
    
    model = FireDetectionTiny().to(args.device)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    criterion = nn.BCELoss()
    
    # Synthetic thermal data generation
    num_samples = 50000
    thermal_frames = torch.randn(num_samples, 1, 24, 32)  # 32×24 thermal array
    gas_features = torch.randn(num_samples, 8)  # lpg, co, nh3, flame, smoke, hist1-3
    
    # Labels: fire if max thermal > 260°C or gas > threshold
    labels = torch.zeros(num_samples)
    for i in range(num_samples):
        max_temp = thermal_frames[i].max().item() * 100 + 50  # Simulated °C
        lpg = gas_features[i, 0].item() * 200 + 100
        if max_temp > 260 or lpg > 800:
            labels[i] = 1.0
    
    # Split
    split = int(num_samples * 0.8)
    train_thermal = thermal_frames[:split].to(args.device)
    train_gas = gas_features[:split].to(args.device)
    train_labels = labels[:split].to(args.device)
    val_thermal = thermal_frames[split:].to(args.device)
    val_gas = gas_features[split:].to(args.device)
    val_labels = labels[split:].to(args.device)
    
    for epoch in range(args.epochs):
        model.train()
        total_loss = 0.0
        
        for i in range(0, split, args.batch_size):
            batch_thermal = train_thermal[i:i+args.batch_size]
            batch_gas = train_gas[i:i+args.batch_size]
            batch_labels = train_labels[i:i+args.batch_size]
            
            optimizer.zero_grad()
            pred = model(batch_thermal, batch_gas)
            loss = criterion(pred, batch_labels)
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item()
        
        avg_loss = total_loss / (split // args.batch_size)
        
        # Validation
        model.eval()
        with torch.no_grad():
            val_pred = model(val_thermal, val_gas)
            val_loss = criterion(val_pred, val_labels).item()
            val_acc = ((val_pred > 0.5) == val_labels).float().mean().item()
        
        print(f"Epoch {epoch+1}/{args.epochs} — Loss: {avg_loss:.4f}, "
              f"Val Loss: {val_loss:.4f}, Val Acc: {val_acc:.4f}")
    
    # Save model
    model_path = Path(args.output_dir) / "fire_detection_tiny.pth"
    torch.save(model.state_dict(), model_path)
    print(f"Fire detection model saved to {model_path}")
    
    # Count parameters
    total_params = sum(p.numel() for p in model.parameters())
    print(f"Model size: {total_params} parameters (~{total_params * 4 / 1024:.1f} KB)")
    print("After TFLite INT8 quantization: ~{:.0f} KB".format(total_params * 0.25 / 1024))


def export_tflite(args):
    """Export trained models to TFLite INT8 for on-device inference."""
    print("Exporting models to TFLite INT8...")
    print("Note: This requires the tensorflow package and trained PyTorch models.")
    print("Use export_tflite.py for detailed conversion.")


# ── Main ───────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FreshKeep ML Training Pipeline")
    parser.add_argument("--model", type=str, required=True,
                       choices=["food_detection", "spoilage", "fire_detection", "all"],
                       help="Which model to train")
    parser.add_argument("--data-dir", type=str, default="./data",
                       help="Path to training data directory")
    parser.add_argument("--output-dir", type=str, default="./models",
                       help="Path to save trained models")
    parser.add_argument("--epochs", type=int, default=10,
                       help="Number of training epochs")
    parser.add_argument("--batch-size", type=int, default=16,
                       help="Training batch size")
    parser.add_argument("--lr", type=float, default=0.001,
                       help="Learning rate")
    parser.add_argument("--device", type=str, default="cuda" if torch.cuda.is_available() else "cpu",
                       help="Training device (cuda/cpu)")
    
    args = parser.parse_args()
    
    os.makedirs(args.output_dir, exist_ok=True)
    
    if args.model == "food_detection":
        train_food_detection(args)
    elif args.model == "spoilage":
        train_spoilage(args)
    elif args.model == "fire_detection":
        train_fire_detection(args)
    elif args.model == "all":
        train_food_detection(args)
        train_spoilage(args)
        train_fire_detection(args)