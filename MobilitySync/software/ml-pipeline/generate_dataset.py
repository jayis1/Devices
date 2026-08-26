from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd

rng = np.random.default_rng(42)
out_dir = Path(__file__).resolve().parent / 'artifacts'
out_dir.mkdir(exist_ok=True)

n = 800
df = pd.DataFrame({
    'asymmetry_pct': rng.normal(12, 8, n).clip(0, 60),
    'unload_rate': rng.normal(13, 4, n).clip(1, 30),
    'retries': rng.integers(0, 4, n),
    'grip_force_n': rng.normal(58, 16, n).clip(5, 150),
    'wheel_speed_rps': rng.normal(2.0, 1.1, n).clip(0, 7),
    'slip_score': rng.normal(0.25, 0.35, n).clip(0, 3),
    'hr_bpm': rng.normal(88, 14, n).clip(45, 160),
    'hrv_proxy': rng.normal(28, 9, n).clip(5, 70),
    'transfer_count': rng.integers(0, 20, n),
    'door_range_m': rng.normal(2.2, 0.9, n).clip(0.1, 6),
    'obstruction': rng.integers(0, 2, n),
    'day_index': rng.integers(0, 56, n),
})

transfer_risk = (df['asymmetry_pct'] * 0.05 + (14 - df['unload_rate']).clip(lower=0) * 0.08 + df['retries'] * 0.18)
walker_risk = df['slip_score'] * 0.9 + (df['wheel_speed_rps'] - df['grip_force_n'] * 0.03).clip(lower=0) * 0.6
fatigue = ((df['hr_bpm'] - 70) * 0.04 + (35 - df['hrv_proxy']).clip(lower=0) * 0.05 + df['transfer_count'] * 0.03 + walker_risk * 0.3)

df['transfer_label'] = np.where(transfer_risk > 1.5, 'unsafe', np.where(transfer_risk > 0.8, 'caution', 'safe'))
df['brake_label'] = (walker_risk > 0.8).astype(int)
df['fatigue_score'] = (fatigue * 35).clip(0, 100)
df['door_open_label'] = ((df['door_range_m'] < 1.8) & (df['obstruction'] == 0)).astype(int)
df['recovery_score'] = (85 - df['asymmetry_pct'] * 0.6 - df['retries'] * 6 - walker_risk * 12 - fatigue * 8 + df['day_index'] * 0.15).clip(0, 100)

path = out_dir / 'mobilitysync_synth.csv'
df.to_csv(path, index=False)
print(f'wrote {path}')
