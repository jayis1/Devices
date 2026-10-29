# Synthetic only: generates example data; not evidence of field performance.
import csv, random
from pathlib import Path
random.seed(7)
with Path(__file__).with_name('synthetic.csv').open('w', newline='') as f:
    w=csv.writer(f); w.writerow(['soil_pct','rain_mm_24h','priority'])
    for _ in range(500):
        soil=random.uniform(5,90); rain=random.uniform(0,25)
        w.writerow([round(soil,2),round(rain,2),int(soil<30 and rain<4)])
