"""
LawnSync ML Pipeline — WeedSeg Training
U-Net-tiny with MobileNetV2 encoder for semantic segmentation.

Classes: Background (grass), Dandelion, Crabgrass, Clover, Thistle,
         Nutsedge, Plantain, Chickweed, Spurge (9 classes)

Output: int8 quantized TFLite model (~1.2 MB) for ESP32-S3 edge inference
"""

import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torchvision import models, transforms
from PIL import Image
import numpy as np

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DATA_DIR = os.environ.get("LAWNSYNC_DATA_DIR", "./data/weed_dataset")
MODEL_SAVE_DIR = "./models"
BATCH_SIZE = 8
NUM_EPOCHS = 80
LEARNING_RATE = 0.0005
IMG_SIZE = 512
NUM_CLASSES = 9

WEED_CLASSES = [
    "Background", "Dandelion", "Crabgrass", "Clover", "Thistle",
    "Nutsedge", "Plantain", "Chickweed", "Spurge"
]


# ---------------------------------------------------------------------------
# Model: U-Net-tiny with MobileNetV2 Encoder
# ---------------------------------------------------------------------------

class SeparableConv2d(nn.Module):
    """Depthwise separable convolution (lightweight)."""
    def __init__(self, in_ch, out_ch, kernel_size=3, padding=1):
        super().__init__()
        self.depthwise = nn.Conv2d(in_ch, in_ch, kernel_size, padding=padding,
                                   groups=in_ch, bias=False)
        self.pointwise = nn.Conv2d(in_ch, out_ch, 1, bias=False)
        self.bn = nn.BatchNorm2d(out_ch)

    def forward(self, x):
        return self.bn(self.pointwise(self.depthwise(x)))


class DecoderBlock(nn.Module):
    """U-Net decoder block with skip connection."""
    def __init__(self, in_ch, skip_ch, out_ch):
        super().__init__()
        self.up = nn.Upsample(scale_factor=2, mode="bilinear", align_corners=False)
        self.conv1 = SeparableConv2d(in_ch + skip_ch, out_ch)
        self.conv2 = SeparableConv2d(out_ch, out_ch)
        self.act = nn.ReLU6(inplace=True)

    def forward(self, x, skip):
        x = self.up(x)
        x = torch.cat([x, skip], dim=1)
        x = self.act(self.conv1(x))
        x = self.act(self.conv2(x))
        return x


class WeedSegNet(nn.Module):
    """U-Net-tiny with MobileNetV2 encoder for weed segmentation.

    Encoder: MobileNetV2 features at different scales
    Decoder: U-Net-style upsampling with skip connections
    """

    def __init__(self, num_classes: int = NUM_CLASSES, pretrained: bool = True):
        super().__init__()
        # MobileNetV2 encoder
        weights = models.MobileNet_V2_Weights.IMAGENET1K_V1 if pretrained else None
        mobilenet = models.mobilenet_v2(weights=weights)
        self.encoder = mobilenet.features

        # Extract skip connection channels from MobileNetV2 layers
        # Layer indices and output channels:
        # 0: 16, 2: 24, 4: 32, 7: 96, 14: 320, 18: 1280
        self.skip_layers = [0, 2, 4, 7, 14]
        self.skip_channels = [16, 24, 32, 96, 320]
        self.bottleneck_channels = 1280  # Last encoder layer

        # Decoder
        self.dec4 = DecoderBlock(1280, 320, 160)
        self.dec3 = DecoderBlock(160, 96, 80)
        self.dec2 = DecoderBlock(80, 32, 40)
        self.dec1 = DecoderBlock(40, 24, 24)
        self.dec0 = DecoderBlock(24, 16, 16)

        # Final classifier
        self.classifier = nn.Sequential(
            nn.Conv2d(16, num_classes, 1),
        )

    def forward(self, x):
        # Encoder with skip extraction
        skips = []
        for i, layer in enumerate(self.encoder):
            x = layer(x)
            if i in self.skip_layers:
                skips.append(x)
        # x is now bottleneck (1280 ch, 16×16 for 512 input)

        # Decoder with skip connections
        d4 = self.dec4(x, skips[4])       # 32×32
        d3 = self.dec3(d4, skips[3])     # 64×64
        d2 = self.dec2(d3, skips[2])     # 128×128
        d1 = self.dec1(d2, skips[1])     # 256×256
        d0 = self.dec0(d1, skips[0])     # 512×512

        out = self.classifier(d0)
        return out


# ---------------------------------------------------------------------------
# Dataset
# ---------------------------------------------------------------------------

class WeedDataset(Dataset):
    """Lawn weed segmentation dataset.

    Expected structure:
        data/weed_dataset/
            train/
                images/  *.jpg
                masks/   *.png (pixel values = class index)
            val/
                images/
                masks/
    """

    def __init__(self, root: str, split: str = "train", size: int = IMG_SIZE):
        self.img_dir = os.path.join(root, split, "images")
        self.mask_dir = os.path.join(root, split, "masks")
        self.size = size
        self.files = [f for f in os.listdir(self.img_dir)
                      if f.endswith((".jpg", ".png"))]
        self.img_transform = transforms.Compose([
            transforms.Resize((size, size)),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406],
                                  std=[0.229, 0.224, 0.225]),
        ])

    def __len__(self):
        return len(self.files)

    def __getitem__(self, idx):
        fname = self.files[idx]
        img = Image.open(os.path.join(self.img_dir, fname)).convert("RGB")
        mask = Image.open(os.path.join(self.mask_dir,
                                       fname.rsplit(".", 1)[0] + ".png"))

        img = img.resize((self.size, self.size), Image.BILINEAR)
        mask = mask.resize((self.size, self.size), Image.NEAREST)

        img = self.img_transform(img)
        mask = np.array(mask, dtype=np.int64)
        mask = torch.from_numpy(mask).long()
        return img, mask


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------

def mIoU(pred: torch.Tensor, target: torch.Tensor, num_classes: int = NUM_CLASSES):
    """Mean Intersection over Union."""
    ious = []
    for c in range(num_classes):
        pred_c = (pred == c)
        target_c = (target == c)
        intersection = (pred_c & target_c).sum().float()
        union = (pred_c | target_c).sum().float()
        if union > 0:
            ious.append((intersection / union).item())
    return np.mean(ious) if ious else 0.0


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train_model():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[WeedSeg] Training on {device}")

    train_ds = WeedDataset(DATA_DIR, "train")
    val_ds = WeedDataset(DATA_DIR, "val")
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False, num_workers=4)

    print(f"[WeedSeg] Train: {len(train_ds)}, Val: {len(val_ds)}")

    model = WeedSegNet(num_classes=NUM_CLASSES, pretrained=True).to(device)
    criterion = nn.CrossEntropyLoss(ignore_index=255)  # 255 = ignore
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=NUM_EPOCHS)

    best_miou = 0
    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)

    for epoch in range(NUM_EPOCHS):
        model.train()
        train_loss = 0.0
        for images, masks in train_loader:
            images, masks = images.to(device), masks.to(device)
            optimizer.zero_grad()
            outputs = model(images)
            loss = criterion(outputs, masks)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * images.size(0)

        scheduler.step()
        train_loss /= len(train_ds)

        # Validate
        model.eval()
        val_loss = 0.0
        val_ious = []
        with torch.no_grad():
            for images, masks in val_loader:
                images, masks = images.to(device), masks.to(device)
                outputs = model(images)
                loss = criterion(outputs, masks)
                val_loss += loss.item() * images.size(0)
                preds = outputs.argmax(1)
                val_ious.append(mIoU(preds, masks))

        val_loss /= len(val_ds)
        val_miou = np.mean(val_ious)

        print(f"Epoch {epoch+1}/{NUM_EPOCHS} — "
              f"Train Loss: {train_loss:.4f}, Val Loss: {val_loss:.4f}, "
              f"Val mIoU: {val_miou:.4f}")

        if val_miou > best_miou:
            best_miou = val_miou
            torch.save(model.state_dict(),
                       os.path.join(MODEL_SAVE_DIR, "weedseg_best.pth"))
            print(f"  → New best mIoU: {val_miou:.4f}")

    print(f"\n[WeedSeg] Best mIoU: {best_miou:.4f}")

    # Export to ONNX
    model.eval()
    model.to("cpu")
    dummy = torch.randn(1, 3, IMG_SIZE, IMG_SIZE)
    onnx_path = os.path.join(MODEL_SAVE_DIR, "weedseg.onnx")
    torch.onnx.export(model, dummy, onnx_path,
                      input_names=["input"], output_names=["output"],
                      opset_version=13)
    print(f"[WeedSeg] Exported ONNX: {onnx_path}")

    return best_miou


if __name__ == "__main__":
    train_model()