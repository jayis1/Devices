"""
BreathHome - Asthma Exacerbation Risk Predictor Training
Transformer-based sequence model for predicting asthma/COPD exacerbation
risk 4-6 hours ahead based on personal exposure history.

Model: Transformer encoder with attention over 72-hour windows
Input: Personal exposure features (PM2.5, CO2, VOC, humidity, temp,
       activity level, symptom logs, medication timing)
Output: Risk level (low/medium/high/critical) and probability
"""

import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, roc_auc_score, confusion_matrix
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import json

# ========== CONFIGURATION ==========

FEATURES = [
    'pm2_5', 'co2', 'voc_index', 'temperature', 'humidity',
    'personal_aqi', 'activity_still', 'activity_walking',
    'activity_running', 'activity_sleeping', 'symptom_wheeze',
    'symptom_cough', 'symptom_sobe', 'symptom_throat',
    'medication_taken', 'hour_of_day', 'day_of_week',
    'pollen_count', 'outdoor_aqi', 'pressure'
]

NUM_FEATURES = len(FEATURES)
SEQUENCE_LENGTH = 72   # 72 hours of 1-hour aggregated data
PREDICTION_HORIZON = 6  # Predict 6 hours ahead
BATCH_SIZE = 64
LEARNING_RATE = 0.0003
EPOCHS = 100
PATIENCE = 10

DATA_PATH = os.getenv("DATA_PATH", "datasets/asthma_exacerbation/")
MODEL_PATH = os.getenv("MODEL_PATH", "models/asthma_risk.pt")


# ========== DATASET ==========

class AsthmaExposureDataset(Dataset):
    """Dataset of personal exposure sequences with asthma exacerbation labels."""
    
    def __init__(self, data_dir: str, sequence_length: int = 72, 
                 prediction_horizon: int = 6):
        self.sequence_length = sequence_length
        self.prediction_horizon = prediction_horizon
        self.sequences = []
        self.labels = []
        self.load_data(data_dir)
    
    def load_data(self, data_dir: str):
        """Load and preprocess exposure data with labels."""
        # In production: load from PostgreSQL
        # For training: load from CSV files
        
        # Simulated data generation for demonstration
        # Real data would come from: sensor_readings + exposure_data + symptom logs
        
        n_patients = 500
        n_days = 90  # 90 days per patient
        
        np.random.seed(42)
        
        for patient in range(n_patients):
            # Generate baseline exposure pattern
            hours = np.arange(n_days * 24)
            
            # Daily patterns
            hour_of_day = hours % 24
            day_of_week = (hours // 24) % 7
            
            # Indoor AQI follows daily pattern
            base_pm25 = 8 + 5 * np.sin(hour_of_day * 2 * np.pi / 24)
            base_co2 = 600 + 300 * np.sin((hour_of_day - 8) * 2 * np.pi / 24)
            base_voc = 80 + 40 * np.sin((hour_of_day - 12) * 2 * np.pi / 24)
            
            # Add random noise
            pm25 = base_pm25 + np.random.normal(0, 3, len(hours))
            co2 = base_co2 + np.random.normal(0, 50, len(hours))
            voc = base_voc + np.random.normal(0, 15, len(hours))
            temp = 22 + 2 * np.sin(hour_of_day * 2 * np.pi / 24) + np.random.normal(0, 0.5, len(hours))
            humidity = 45 + 10 * np.sin((hour_of_day - 6) * 2 * np.pi / 24) + np.random.normal(0, 3, len(hours))
            
            # Activity patterns
            activity = np.zeros((len(hours), 4))  # still, walking, running, sleeping
            for i, h in enumerate(hour_of_day):
                if h < 7 or h > 22:
                    activity[i] = [0.1, 0, 0, 0.9]  # sleeping
                elif 9 <= h <= 17:
                    activity[i] = [0.5, 0.4, 0.1, 0]  # active day
                else:
                    activity[i] = [0.7, 0.2, 0.05, 0.05]  # evening
            
            # Personal AQI
            personal_aqi = np.maximum(0, 
                pm25 * 2 + (co2 - 400) * 0.05 + voc * 0.3 + 
                np.random.normal(0, 10, len(hours)))
            
            # Symptom events (correlated with high exposure)
            symptoms = np.zeros((len(hours), 4))  # wheeze, cough, sobe, throat
            for i in range(len(hours)):
                if personal_aqi[i] > 120:
                    prob = 0.3 * (personal_aqi[i] - 120) / 100
                    if np.random.random() < prob:
                        symptoms[i] = [0.3, 0.5, 0.1, 0.1]  # cough most common
                        symptoms[i] = np.random.dirichlet(symptoms[i] * 10 + 0.1)
            
            # Medication (taken after symptoms)
            medication = np.zeros(len(hours))
            for i in range(1, len(hours)):
                if np.any(symptoms[i-1] > 0.3) and np.random.random() < 0.7:
                    medication[i] = 1.0
            
            # Exacerbation labels (defined as: significant symptom event requiring 
            # medication or medical attention)
            exacerbation = np.zeros(len(hours))
            for i in range(len(hours)):
                # Exacerbation risk increases with cumulative exposure
                if i >= self.prediction_horizon:
                    window = personal_aqi[i-self.prediction_horizon:i]
                    high_exposure = np.mean(window > 100)
                    symptom_density = np.sum(symptoms[i-self.prediction_horizon:i] > 0.3) / self.prediction_horizon
                    exacerbation[i] = 1.0 if (high_exposure > 0.5 and symptom_density > 0.3) else 0.0
            
            # Build sequences
            for i in range(self.sequence_length, len(hours) - self.prediction_horizon):
                seq = np.zeros((self.sequence_length, NUM_FEATURES))
                for j in range(self.sequence_length):
                    idx = i - self.sequence_length + j
                    seq[j, 0] = pm25[idx]
                    seq[j, 1] = co2[idx]
                    seq[j, 2] = voc[idx]
                    seq[j, 3] = temp[idx]
                    seq[j, 4] = humidity[idx]
                    seq[j, 5] = personal_aqi[idx]
                    seq[j, 6:10] = activity[idx]
                    seq[j, 10:14] = symptoms[idx]
                    seq[j, 14] = medication[idx]
                    seq[j, 15] = (hour_of_day[idx]) / 24.0
                    seq[j, 16] = day_of_week[idx] / 7.0
                    seq[j, 17] = np.random.uniform(0, 10)  # pollen (external)
                    seq[j, 18] = np.random.uniform(20, 80)  # outdoor AQI (external)
                    seq[j, 19] = 1013 + np.random.normal(0, 5)  # pressure
                
                label = exacerbation[i + self.prediction_horizon]
                self.sequences.append(seq)
                self.labels.append(label)
    
    def __len__(self):
        return len(self.sequences)
    
    def __getitem__(self, idx):
        return (
            torch.FloatTensor(self.sequences[idx]),
            torch.FloatTensor([self.labels[idx]])
        )


# ========== MODEL ==========

class AsthmaRiskTransformer(nn.Module):
    """Transformer-based asthma exacerbation risk predictor."""
    
    def __init__(self, num_features: int = NUM_FEATURES, 
                 d_model: int = 128, nhead: int = 8, 
                 num_layers: int = 4, dim_feedforward: int = 512,
                 dropout: float = 0.1):
        super().__init__()
        
        # Input projection
        self.input_proj = nn.Linear(num_features, d_model)
        
        # Positional encoding
        self.pos_encoder = nn.Parameter(torch.randn(1, 72, d_model) * 0.02)
        
        # Transformer encoder
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=d_model, nhead=nhead,
            dim_feedforward=dim_feedforward,
            dropout=dropout, batch_first=True
        )
        self.transformer = nn.TransformerEncoder(encoder_layer, num_layers=num_layers)
        
        # Classification head
        self.classifier = nn.Sequential(
            nn.Linear(d_model, 64),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(32, 4)  # 4 classes: low, medium, high, critical
        )
        
        # Risk probability head (regression)
        self.risk_head = nn.Sequential(
            nn.Linear(d_model, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
            nn.Sigmoid()
        )
        
        self.d_model = d_model
    
    def forward(self, x):
        """Forward pass.
        
        Args:
            x: Input tensor of shape (batch, seq_len, num_features)
        
        Returns:
            risk_class: Classification logits (batch, 4)
            risk_prob: Risk probability (batch, 1)
        """
        # Project input features to d_model dimensions
        x = self.input_proj(x)  # (batch, seq_len, d_model)
        
        # Add positional encoding
        x = x + self.pos_encoder[:, :x.size(1), :]
        
        # Transformer encoding
        x = self.transformer(x)  # (batch, seq_len, d_model)
        
        # Use last timestep output for classification
        x_last = x[:, -1, :]  # (batch, d_model)
        
        # Also use mean pooling for context
        x_mean = x.mean(dim=1)  # (batch, d_model)
        
        # Combine
        x_combined = (x_last + x_mean) / 2
        
        # Outputs
        risk_class = self.classifier(x_combined)  # (batch, 4)
        risk_prob = self.risk_head(x_combined)      # (batch, 1)
        
        return risk_class, risk_prob


# ========== TRAINING ==========

def train_model():
    """Train the asthma risk prediction model."""
    
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on {device}")
    
    # Load dataset
    print("Loading dataset...")
    dataset = AsthmaExposureDataset(DATA_PATH)
    
    # Split into train/val/test
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
    model = AsthmaRiskTransformer().to(device)
    
    # Loss functions
    class_criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 3.0, 5.0, 10.0]).to(device))
    prob_criterion = nn.BCELoss()
    
    # Optimizer with weight decay
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=0.01)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min', patience=5, factor=0.5)
    
    # Training loop
    best_val_loss = float('inf')
    patience_counter = 0
    
    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0.0
        
        for batch_idx, (sequences, labels) in enumerate(train_loader):
            sequences = sequences.to(device)
            labels_class = labels.squeeze().long().to(device)
            labels_prob = labels.to(device)
            
            optimizer.zero_grad()
            
            risk_class, risk_prob = model(sequences)
            
            # Combined loss: classification + probability
            loss_class = class_criterion(risk_class, labels_class)
            loss_prob = prob_criterion(risk_prob, labels_prob)
            loss = loss_class + 0.5 * loss_prob
            
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimizer.step()
            
            train_loss += loss.item()
        
        avg_train_loss = train_loss / len(train_loader)
        
        # Validation
        model.eval()
        val_loss = 0.0
        all_preds = []
        all_labels = []
        all_probs = []
        
        with torch.no_grad():
            for sequences, labels in val_loader:
                sequences = sequences.to(device)
                labels_class = labels.squeeze().long().to(device)
                labels_prob = labels.to(device)
                
                risk_class, risk_prob = model(sequences)
                
                loss_class = class_criterion(risk_class, labels_class)
                loss_prob = prob_criterion(risk_prob, labels_prob)
                loss = loss_class + 0.5 * loss_prob
                
                val_loss += loss.item()
                
                preds = risk_class.argmax(dim=1).cpu().numpy()
                all_preds.extend(preds)
                all_labels.extend(labels_class.cpu().numpy())
                all_probs.extend(risk_prob.cpu().numpy())
        
        avg_val_loss = val_loss / len(val_loader)
        
        # Scheduler step
        scheduler.step(avg_val_loss)
        
        # Print metrics
        print(f"Epoch {epoch+1}/{EPOCHS} - "
              f"Train Loss: {avg_train_loss:.4f}, "
              f"Val Loss: {avg_val_loss:.4f}")
        
        # Early stopping
        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            patience_counter = 0
            torch.save(model.state_dict(), MODEL_PATH)
            print(f"  → Saved best model (val_loss={avg_val_loss:.4f})")
        else:
            patience_counter += 1
            if patience_counter >= PATIENCE:
                print(f"Early stopping at epoch {epoch+1}")
                break
    
    # Evaluate on test set
    print("\n=== Test Set Evaluation ===")
    model.load_state_dict(torch.load(MODEL_PATH))
    model.eval()
    
    all_preds = []
    all_labels = []
    all_probs = []
    
    with torch.no_grad():
        for sequences, labels in test_loader:
            sequences = sequences.to(device)
            labels_class = labels.squeeze().long()
            
            risk_class, risk_prob = model(sequences)
            
            preds = risk_class.argmax(dim=1).cpu().numpy()
            all_preds.extend(preds)
            all_labels.extend(labels_class.numpy())
            all_probs.extend(risk_prob.cpu().numpy())
    
    target_names = ['low', 'medium', 'high', 'critical']
    print("\nClassification Report:")
    print(classification_report(all_labels, all_preds, target_names=target_names))
    
    print(f"\nAUC-ROC: {roc_auc_score(all_labels, all_probs, multi_class='ovr'):.4f}")
    
    # Confusion matrix
    cm = confusion_matrix(all_labels, all_preds)
    print(f"\nConfusion Matrix:\n{cm}")
    
    # Export to TFLite for edge deployment
    print("\n=== Exporting for Edge Deployment ===")
    model.eval()
    
    # Export to ONNX first
    dummy_input = torch.randn(1, SEQUENCE_LENGTH, NUM_FEATURES)
    torch.onnx.export(
        model, dummy_input,
        MODEL_PATH.replace('.pt', '.onnx'),
        input_names=['input'],
        output_names=['risk_class', 'risk_prob'],
        dynamic_axes={'input': {0: 'batch_size'}, 
                      'risk_class': {0: 'batch_size'},
                      'risk_prob': {0: 'batch_size'}},
        opset_version=13
    )
    
    print(f"\nModel saved to {MODEL_PATH}")
    print(f"ONNX model saved to {MODEL_PATH.replace('.pt', '.onnx')}")
    print("Convert to TFLite using: onnx2tf + tf2tflite")
    print("Or use PyTorch → TFLite via onnx-tf pipeline")


if __name__ == "__main__":
    train_model()