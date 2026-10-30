import csv
from pathlib import Path

OUT = Path(__file__).with_name('synthetic.csv')
with OUT.open('w', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow(['bin_mass_g', 'passages_24h', 'inspection_priority'])
    for mass in range(0, 101, 10):
        for passages in (0, 5, 20):
            writer.writerow([mass * 100, passages, int(mass >= 80 or (mass >= 60 and passages > 10))])
print(f'Wrote illustrative synthetic fixture: {OUT}')
