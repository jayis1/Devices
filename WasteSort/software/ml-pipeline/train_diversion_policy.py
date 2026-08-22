from __future__ import annotations

import random
from dataclasses import dataclass


@dataclass
class Arm:
    name: str
    expected_gain: float


ARMS = [
    Arm("rinse_tip", 0.08),
    Arm("bulk_buy_suggestion", 0.11),
    Arm("pickup_reminder", 0.14),
    Arm("compost_browns_tip", 0.07),
]


def simulate(rounds: int = 200) -> dict[str, float]:
    scores = {arm.name: 0.0 for arm in ARMS}
    counts = {arm.name: 1 for arm in ARMS}
    for arm in ARMS:
        scores[arm.name] = arm.expected_gain + random.random() * 0.02
    for _ in range(rounds):
        choice = max(ARMS, key=lambda arm: scores[arm.name] / counts[arm.name])
        reward = max(0.0, random.gauss(choice.expected_gain, 0.05))
        scores[choice.name] += reward
        counts[choice.name] += 1
    return {name: round(scores[name] / counts[name], 4) for name in scores}


if __name__ == "__main__":
    print(simulate())
