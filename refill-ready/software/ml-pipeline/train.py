import json
from pathlib import Path

def score(flow_pulses, leak, route_code):
    # Demonstration heuristic only; thresholds are not safety claims.
    return min(1.0, (0.75 if flow_pulses == 0 else 0.0) + (0.8 if leak else 0.0) + (0.3 if route_code == 2 else 0.0))

def main():
    out = Path(__file__).with_name('synthetic.jsonl')
    with out.open('w') as f:
        for flow in (0, 1, 8):
            for leak in (0, 1):
                row = {'flow_pulses_10s': flow, 'leak': leak, 'inspection_score': score(flow, leak, 0), 'synthetic': True}
                f.write(json.dumps(row) + '\n')
    print(f'Wrote synthetic-only fixture: {out}')

if __name__ == '__main__': main()
