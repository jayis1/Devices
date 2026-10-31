"""Synthetic CSV smoke generation only; not model training or evaluation."""
import csv
from pathlib import Path
def generate(path:Path,count:int=20)->None:
 with path.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=['sample_mA','label']);w.writeheader()
  for i in range(count):w.writerow({'sample_mA':300 if i%2 else 20,'label':'active' if i%2 else 'idle'})
if __name__=='__main__':generate(Path('synthetic_smoke.csv'))
