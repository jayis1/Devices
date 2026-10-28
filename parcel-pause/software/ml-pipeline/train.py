#!/usr/bin/env python3
"""Deterministic synthetic-data pipeline smoke test; not a validated predictor."""
import random
random.seed(11)
data=[(x, int(x > 0.7)) for x in [random.random() for _ in range(100)]]
train, test=data[:80], data[80:]
threshold=0.7
correct=sum((x > threshold)==bool(y) for x,y in test)
print(f'synthetic smoke test: {len(train)} train, {len(test)} test, {correct} correct')
