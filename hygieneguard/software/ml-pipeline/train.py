"""Synthetic refill-risk smoke baseline. Author: jayis1."""
from math import exp
from pathlib import Path
from random import Random

random = Random(17)
examples = []
for _ in range(400):
    soap_g, flow_ml, temp_c = random.uniform(0, 500), random.uniform(0, 1200), random.uniform(15, 35)
    label = int(soap_g < 110 or (soap_g < 180 and flow_ml > 700))
    examples.append(([1.0, soap_g / 500, flow_ml / 1200, temp_c / 35], label))
train, test = examples[:300], examples[300:]
weights = [0.0] * 4
for _ in range(500):
    for features, label in train:
        score = sum(weight * value for weight, value in zip(weights, features))
        prediction = 1 / (1 + exp(-max(-30, min(30, score))))
        for index, value in enumerate(features):
            weights[index] -= 0.08 * (prediction - label) * value
correct = sum(int((1 / (1 + exp(-sum(w * x for w, x in zip(weights, features))))) >= 0.5) == label for features, label in test)
print({"synthetic_accuracy": round(correct / len(test), 3), "note": "smoke baseline only; not a hygiene or health model"})
Path(__file__).with_name("model-card.txt").write_text("Synthetic refill-risk smoke model; not for health, compliance, or clinical decisions. Author: jayis1.\n")
