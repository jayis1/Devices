# ML experiment

Schema: rh_pct, temp_c, high_rh_advisory. `generate.py` creates deterministic synthetic examples; `train.py` evaluates a fixed RH≥65% rule on the final 20% split. This is a pipeline smoke test, not training or evidence of seed quality/viability. The system ships the explicit humidity threshold only. Future real data requires consent, site-specific calibrated loggers, volunteer labels, leakage-resistant site/time holdouts, and agronomist review.
