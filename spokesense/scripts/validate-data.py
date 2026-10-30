import csv, json
from pathlib import Path
root=Path(__file__).parent
for p in root.rglob('*.json'):
    json.loads(p.read_text())
for p in root.rglob('*.csv'):
    rows=list(csv.reader(p.open(newline='')))
    assert rows and all(len(row)==len(rows[0]) for row in rows), p
    assert {'RefDes','Qty','Value/Description','Manufacturer','MPN','Package','Unit Cost USD'} <= set(rows[0]), p
print('JSON and BOM CSV structures valid')
