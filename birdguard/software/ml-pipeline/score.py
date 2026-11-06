#!/usr/bin/env python3
import argparse, csv, json

def score(row):
    # Heuristic requiring inspection; never a collision probability.
    return min(100, (35 if row['panel_present'].lower()=='false' else 0) + min(40, float(row['lux'])/2500) + min(25, max(0, float(row['humidity_pct'])-70)))
if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('--input',required=True); a=p.parse_args()
    with open(a.input,newline='') as f:
        for r in csv.DictReader(f): print(json.dumps({'window_id':r['window_id'],'inspection_priority':round(score(r),1),'meaning':'heuristic; inspect glass and markers'}))
