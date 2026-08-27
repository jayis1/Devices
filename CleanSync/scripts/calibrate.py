from __future__ import annotations

import json
from statistics import mean

sample_reflectance = [0.11, 0.12, 0.10, 0.09, 0.11]
sample_fluorescence = [8, 10, 7, 9, 8]

baseline = {
    'floor_reflectance_baseline': round(mean(sample_reflectance), 3),
    'wand_clean_fluorescence_baseline': round(mean(sample_fluorescence), 2),
}

print(json.dumps(baseline, indent=2))
