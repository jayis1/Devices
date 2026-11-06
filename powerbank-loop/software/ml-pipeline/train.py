import argparse, csv, json
from pathlib import Path

def train(path: Path):
    with path.open(newline='') as f: rows=list(csv.DictReader(f))
    if not rows: raise ValueError('training data is empty')
    required={'timestamp','borrow_count','return_count'}
    if not required.issubset(rows[0]): raise ValueError('missing required columns')
    values=[int(r['borrow_count'])+int(r['return_count']) for r in rows]
    return {'model':'constant-mean-baseline','rows':len(values),'mean_hourly_events':sum(values)/len(values),'claim':'synthetic mechanics only; not a validated forecast'}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('input',type=Path,nargs='?',default=Path(__file__).with_name('synthetic.csv'));p.add_argument('--output',type=Path);a=p.parse_args();result=train(a.input);text=json.dumps(result,indent=2);print(text)
    if a.output:a.output.write_text(text+'\n')
