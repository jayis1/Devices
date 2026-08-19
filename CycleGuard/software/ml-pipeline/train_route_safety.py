"""
RouteSafety GCN Training Script
Per-segment route safety scoring using Graph Convolutional Network.

Input: Road network graph (nodes = intersections, edges = segments)
       + historical crash data + weather + time of day features
Output: Per-segment safety score (0-100)
Architecture: 2-layer Graph Convolutional Network (64-dim embeddings)

Uses PyTorch Geometric (torch_geometric) for graph operations.
"""

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

# In production: from torch_geometric.nn import GCNConv
# Simplified: use dense adjacency matrix for illustration


class RouteSafetyGCN(nn.Module):
    def __init__(self, node_features=16, hidden_dim=64):
        super().__init__()
        # GCN layers (simplified as linear + adjacency multiplication)
        self.fc1 = nn.Linear(node_features, hidden_dim)
        self.fc2 = nn.Linear(hidden_dim, hidden_dim)
        self.fc3 = nn.Linear(hidden_dim, 1)  # per-node safety score

    def forward(self, x, adj):
        """
        x: (num_nodes, node_features) — road segment features
        adj: (num_nodes, num_nodes) — adjacency matrix
        Returns: (num_nodes,) safety scores (0-1)
        """
        # GCN layer 1: A * X * W
        h = torch.relu(self.fc1(adj @ x))
        h = torch.relu(self.fc2(adj @ h))
        scores = torch.sigmoid(self.fc3(adj @ h)).squeeze(-1)
        return scores


class RouteSafetyDataset:
    def __init__(self, num_graphs=100, max_nodes=500):
        # In production: load from OSM + historical crash data
        np.random.seed(42)
        self.graphs = []
        for _ in range(num_graphs):
            n = np.random.randint(50, max_nodes)
            # Node features: [road_type, speed_limit, lanes, bike_lane,
            #   intersection, traffic, lighting, surface, crash_count,
            #   weather_rain, weather_wind, time_rush, time_night,
            #   bike_traffic, avg_speed, width]
            x = np.random.randn(n, 16).astype(np.float32)
            # Random sparse adjacency matrix
            adj = np.zeros((n, n), dtype=np.float32)
            for i in range(n - 1):
                adj[i][i + 1] = 1.0
                adj[i + 1][i] = 1.0
                if np.random.random() > 0.7:
                    j = min(i + np.random.randint(2, 10), n - 1)
                    adj[i][j] = 1.0
                    adj[j][i] = 1.0
            # Normalize adjacency (D^-0.5 * A * D^-0.5)
            degree = adj.sum(axis=1)
            d_inv_sqrt = np.power(degree, -0.5, where=degree > 0)
            d_mat = np.diag(d_inv_sqrt)
            adj_norm = d_mat @ adj @ d_mat
            # Labels: safety score (0-1) — higher = safer
            y = np.random.rand(n).astype(np.float32)
            self.graphs.append((torch.tensor(x), torch.tensor(adj_norm),
                                torch.tensor(y)))

    def __len__(self): return len(self.graphs)
    def __getitem__(self, idx): return self.graphs[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = RouteSafetyGCN().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.MSELoss()

    dataset = RouteSafetyDataset()

    best_loss = float('inf')
    for epoch in range(50):
        model.train()
        total_loss = 0
        for x, adj, y in dataset:
            x, adj, y = x.to(device), adj.to(device), y.to(device)
            optimizer.zero_grad()
            pred = model(x, adj)
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()

        avg_loss = total_loss / len(dataset)
        print(f"Epoch {epoch+1}: avg_loss={avg_loss:.6f}")

        if avg_loss < best_loss:
            best_loss = avg_loss
            torch.save(model.state_dict(), "route_safety_best.pt")

    # Export (GCN export requires dynamic input sizes — use TorchScript)
    model.load_state_dict(torch.load("route_safety_best.pt"))
    model.eval()
    model_scripted = torch.jit.script(model)
    model_scripted.save("route_safety.pt")
    print(f"RouteSafety GCN trained. Best loss: {best_loss:.6f}")
    print("Exported: route_safety.pt (TorchScript for cloud inference)")


if __name__ == "__main__":
    train()