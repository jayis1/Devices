# ML demonstration only

Input CSV rows should be generated from consented, calibrated, site-approved time buckets. Label `relatively_quieter` as a lower measured median bucket among simultaneously sampled route segments (not a subjective comfort or health label). Keep whole days/sites separated between train and evaluation to avoid temporal leakage. Report MAE against calibrated references and pairwise ranking accuracy. Synthetic generator is for pipeline smoke tests only; no field dataset, trained model or validated inference is included.
