"""
BreathHome - Mold Growth Risk Prediction Model Training
1D-CNN + GRU model for predicting mold growth risk from environmental data.
Runs on room sensor nodes (TFLite Micro, 45KB) and cloud.
"""

import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, roc_auc_score
import matplotlib.pyplot as plt

# ========== CONFIGURATION ==========

FEATURES = ['humidity', 'voc_index', 'temperature', 'dew_point']
NUM_FEATURES = len(FEATURES)
SEQUENCE_LENGTH = 96   # 24 hours of 15-minute data (96 samples)
BATCH_SIZE = 128
LEARNING_RATE = 0.001
EPOCHS = 50
PATIENCE = 8

DATA_PATH = os.getenv("DATA_PATH", "datasets/mold_outcomes/")
MODEL_PATH = os.getenv("MODEL_PATH", "models/mold_risk_model.tflite")


# ========== DATASET ==========

class MoldRiskDataset(Dataset):
    """Environmental data with confirmed mold growth labels."""
    
    def __init__(self, n_samples: int = 10000, sequence_length: int = 96):
        self.sequence_length = sequence_length
        self.sequences = []
        self.labels = []
        self._generate_synthetic_data(n_samples)
    
    def _generate_synthetic_data(self, n_samples: int):
        """Generate synthetic environmental data with mold growth labels.
        
        In production, this would load from real sensor data + confirmed
        mold growth reports (visual inspection + spore count).
        """
        np.random.seed(42)
        
        for _ in range(n_samples):
            # Generate 24 hours of 15-minute data
            seq = np.zeros((self.sequence_length, NUM_FEATURES))
            
            # Random base conditions
            base_temp = np.random.uniform(15, 30)
            base_humidity = np.random.uniform(30, 95)
            base_voc = np.random.uniform(50, 400)
            
            # Add daily patterns
            hours = np.linspace(0, 24, self.sequence_length)
            temp = base_temp + 3 * np.sin((hours - 6) * 2 * np.pi / 24) + np.random.normal(0, 0.5, self.sequence_length)
            humidity = base_humidity + 15 * np.sin((hours - 4) * 2 * np.pi / 24) + np.random.normal(0, 3, self.sequence_length)
            voc = base_voc + 50 * np.sin((hours - 12) * 2 * np.pi / 24) + np.random.normal(0, 15, self.sequence_length)
            
            # Calculate dew point
            a = 17.27
            b = 237.7
            alpha = (a * temp) / (b + temp) + np.log(np.clip(humidity, 1, 100) / 100)
            dew_point = (b * alpha) / (a - alpha)
            
            # Build sequence
            seq[:, 0] = np.clip(humidity, 0, 100)
            seq[:, 1] = np.clip(voc, 0, 500)
            seq[:, 2] = temp
            seq[:, 3] = dew_point
            
            # Mold growth label: probability based on conditions
            # Mold grows when: humidity > 60%, temp 10-35°C, sustained for hours
            wet_hours = np.sum(humidity > 70) / 4  # Convert samples to hours
            optimal_temp = np.sum((temp > 20) & (temp < 30)) / 4
            high_voc = np.mean(voc > 200)
            
            # Mold risk score
            risk = 0.0
            if wet_hours > 6:
                risk += 30 * min(wet_hours / 12, 1)
            if optimal_temp > 4:
                risk += 30 * min(optimal_temp / 8, 1)
            if high_voc > 0.3:
                risk += 20 * min(high_voc, 1)
            if np.max(humidity) > 85:
                risk += 20
            
            risk = min(risk, 100)
            
            # Add some noise
            risk += np.random.normal(0, 5)
            risk = np.clip(risk, 0, 100)
            
            self.sequences.append(seq)
            self.labels.append(risk / 100.0)  # Normalize to 0-1
    
    def __len__(self):
        return len(self.sequences)
    
    def __getitem__(self, idx):
        return (
            torch.FloatTensor(self.sequences[idx]),
            torch.FloatTensor([self.labels[idx]])
        )


# ========== MODEL ==========

class MoldRiskModel(nn.Module):
    """1D-CNN + GRU model for mold growth risk prediction.
    
    Designed to be small enough for TFLite Micro on room sensor nodes (45KB).
    """
    
    def __init__(self, num_features: int = NUM_FEATURES, 
                 hidden_size: int = 32, num_layers: int = 1):
        super().__init__()
        
        # 1D-CNN feature extraction
        self.conv1 = nn.Conv1d(num_features, 16, kernel_size=3, padding=1)
        self.conv2 = nn.Conv1d(16, 32, kernel_size=3, padding=1)
        self.bn1 = nn.BatchNorm1d(16)
        self.bn2 = nn.BatchNorm1d(32)
        
        # GRU for temporal modeling
        self.gru = nn.GRU(32, hidden_size, num_layers=num_layers, 
                          batch_first=True, dropout=0.1)
        
        # Classification head
        self.fc = nn.Sequential(
            nn.Linear(hidden_size, 16),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(16, 1),
            nn.Sigmoid()
        )
    
    def forward(self, x):
        """Forward pass.
        
        Args:
            x: Input tensor of shape (batch, seq_len, num_features)
        
        Returns:
            risk: Mold risk probability (batch, 1)
        """
        # Transpose for Conv1d: (batch, features, seq_len)
        x = x.transpose(1, 2)
        
        # 1D-CNN feature extraction
        x = torch.relu(self.bn1(self.conv1(x)))
        x = torch.relu(self.bn2(self.conv2(x)))
        
        # Transpose back for GRU: (batch, seq_len, features)
        x = x.transpose(1, 2)
        
        # GRU temporal modeling
        x, _ = self.gru(x)
        
        # Use last output
        x = x[:, -1, :]
        
        # Classification
        risk = self.fc(x)
        
        return risk


# ========== TRAINING ==========

def train_model():
    """Train the mold risk prediction model."""
    
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on {device}")
    
    # Load dataset
    print("Loading dataset...")
    dataset = MoldRiskDataset(n_samples=10000)
    
    # Split
    train_size = int(0.7 * len(dataset))
    val_size = int(0.15 * len(dataset))
    test_size = len(dataset) - train_size - val_size
    
    train_dataset, val_dataset, test_dataset = torch.utils.data.random_split(
        dataset, [train_size, val_size, test_size]
    )
    
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE)
    test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE)
    
    # Initialize model
    model = MoldRiskModel().to(device)
    
    # Count parameters
    total_params = sum(p.numel() for p in model.parameters())
    print(f"Model parameters: {total_params:,}")
    print(f"Estimated TFLite size: ~{total_params * 4 / 1024:.0f}KB (float32) or ~{total_params / 1024:.0f}KB (int8)")
    
    # Loss and optimizer
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=5, factor=0.5)
    
    # Training loop
    best_val_loss = float('inf')
    patience_counter = 0
    
    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0.0
        
        for sequences, labels in train_loader:
            sequences = sequences.to(device)
            labels = labels.to(device)
            
            optimizer.zero_grad()
            outputs = model(sequences)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            
            train_loss += loss.item()
        
        avg_train_loss = train_loss / len(train_loader)
        
        # Validation
        model.eval()
        val_loss = 0.0
        all_preds = []
        all_labels = []
        
        with torch.no_grad():
            for sequences, labels in val_loader:
                sequences = sequences.to(device)
                labels = labels.to(device)
                
                outputs = model(sequences)
                loss = criterion(outputs, labels)
                val_loss += loss.item()
                
                all_preds.extend(outputs.cpu().numpy().flatten())
                all_labels.extend(labels.cpu().numpy().flatten())
        
        avg_val_loss = val_loss / len(val_loader)
        scheduler.step(avg_val_loss)
        
        # Compute metrics
        mae = np.mean(np.abs(np.array(all_preds) - np.array(all_labels)))
        
        print(f"Epoch {epoch+1}/{EPOCHS} - "
              f"Train Loss: {avg_train_loss:.4f}, "
              f"Val Loss: {avg_val_loss:.4f}, "
              f"MAE: {mae:.4f}")
        
        # Early stopping
        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            patience_counter = 0
            torch.save(model.state_dict(), "models/mold_risk.pt")
        else:
            patience_counter += 1
            if patience_counter >= PATIENCE:
                print(f"Early stopping at epoch {epoch+1}")
                break
    
    # Final evaluation
    print("\n=== Test Set Evaluation ===")
    model.load_state_dict(torch.load("models/mold_risk.pt"))
    model.eval()
    
    all_preds = []
    all_labels = []
    
    with torch.no_grad():
        for sequences, labels in test_loader:
            sequences = sequences.to(device)
            outputs = model(sequences)
            all_preds.extend(outputs.cpu().numpy().flatten())
            all_labels.extend(labels.cpu().numpy().flatten())
    
    preds = np.array(all_preds)
    labels = np.array(all_labels)
    
    # Binary classification at 60% threshold
    pred_binary = (preds > 0.6).astype(int)
    label_binary = (labels > 0.6).astype(int)
    
    print(f"\nRegression MAE: {np.mean(np.abs(preds - labels)):.4f}")
    print(f"Correlation: {np.corrcoef(preds, labels)[0, 1]:.4f}")
    print(f"\nBinary Classification (threshold=60%):")
    print(classification_report(label_binary, pred_binary, target_names=['low_risk', 'high_risk']))
    
    # Export quantized model for TFLite Micro
    print("\n=== Exporting TFLite Model ===")
    model.eval()
    model.cpu()
    
    # Trace model for export
    dummy_input = torch.randn(1, SEQUENCE_LENGTH, NUM_FEATURES)
    traced_model = torch.jit.trace(model, dummy_input)
    traced_model.save("models/mold_risk_traced.pt")
    
    print("Model exported for TFLite conversion.")
    print("Use: torch → ONNX → TFLite pipeline for deployment on room sensor nodes.")
    print(f"Target size: ~45KB (int8 quantized)")


if __name__ == "__main__":
    train_model()