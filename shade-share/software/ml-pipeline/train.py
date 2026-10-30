import argparse
import csv
import random
from pathlib import Path

def main():
    p=argparse.ArgumentParser(); p.add_argument('--synthetic', action='store_true'); p.add_argument('--out', default='synthetic.csv'); a=p.parse_args()
    if not a.synthetic: raise SystemExit('Only synthetic demo data is supported; no field dataset included')
    random.seed(7)
    with Path(a.out).open('w', newline='') as f:
        w=csv.writer(f); w.writerow(['temp_c','tamper','inspection_priority'])
        for _ in range(200):
            t=random.uniform(15,65); tamper=random.randrange(2); w.writerow([round(t,1),tamper,int(t>45 or tamper)])
if __name__=='__main__': main()
