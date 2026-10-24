import csv
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
files=sorted((ROOT/'hardware/bom').glob('*-BOM.csv'))
assert len(files)==3
headers=[]
for file in files:
    with file.open(newline='') as stream:
        rows=list(csv.DictReader(stream))
    assert rows and all(set(r)=={'ref','qty','part_number','manufacturer','description','unit_cost_usd','supplier'} for r in rows)
    assert all(r['part_number'].strip() and int(r['qty'])>0 for r in rows)
    assert all(float(r['unit_cost_usd'])>=0 for r in rows)
    headers.append(tuple(rows[0].keys()))
assert len(set(headers))==1
# Standard-library-only check of shared frame length and docs API route coverage.
header=(ROOT/'firmware/common/protocol.h').read_text()
assert '#define FL_FRAME_LEN 23u' in header
api=(ROOT/'docs/api.md').read_text()
source=(ROOT/'software/dashboard/main.py').read_text()
for route in ('/health','/api/v1/telemetry','/api/v1/sites/{site_id}/latest'):
    assert route in api and route in source
print(f'validated {len(files)} BOM CSVs and documented API routes')
