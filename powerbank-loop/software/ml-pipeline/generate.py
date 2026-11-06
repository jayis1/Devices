import csv, pathlib, random
import argparse
from collections import defaultdict
from datetime import datetime, timedelta

def generate(path: pathlib.Path, rows: int=500):
    rng=random.Random(17); start=datetime(2025,1,1); counts=defaultdict(int)
    with path.open('w',newline='') as f:
        w=csv.writer(f); w.writerow(['timestamp','borrow_count','return_count'])
        for i in range(rows):
            borrow=max(0,round(3+2*(i%24 in range(8,18))+rng.gauss(0,1)))
            ret=max(0,round(2+1.5*(i%24 in range(16,24))+rng.gauss(0,1)))
            w.writerow([(start+timedelta(hours=i)).isoformat(),borrow,ret]); counts['borrow']+=borrow;counts['return']+=ret
    return dict(counts)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=pathlib.Path,default=pathlib.Path(__file__).with_name('synthetic.csv'));args=parser.parse_args()
    print(generate(args.output))
