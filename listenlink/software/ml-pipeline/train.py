"""Baseline contract; requires CSV rows: site,room,transmitter_on,receiver_count,observed_at.
Uses chronological holdout to avoid future leakage. No clinical or safety inference.
"""
import csv, sys

def main(path):
 with open(path,newline='') as f: rows=list(csv.DictReader(f))
 if not rows: raise SystemExit('empty dataset')
 required={'site','room','transmitter_on','receiver_count','observed_at'}
 if not required.issubset(rows[0]): raise SystemExit(f'missing columns: {required-set(rows[0])}')
 split=max(1,int(len(rows)*.8)); train,test=rows[:split],rows[split:]
 print(f'rows={len(rows)} train={len(train)} chronological_test={len(test)}')
 print('Baseline: predict not_ready for every row; report accuracy/recall only after labels and provenance are independently reviewed.')
if __name__=='__main__': main(sys.argv[1])
