# Synthetic baseline only; not evidence of real-world drying accuracy.
import csv
import random

random.seed(17)
with open("synthetic.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["elapsed_min", "mass_loss_g", "rh_pct", "remaining_min"])
    for _ in range(300):
        duration = random.randint(90, 600)
        t = random.randint(0, duration)
        loss = int(t * random.uniform(0.2, 1.5))
        rh = random.randint(30, 85)
        w.writerow([t, loss, rh, max(0, duration-t)])
print("generated synthetic.csv (illustrative only)")
