import csv
from pathlib import Path
for path in sorted(Path(__file__).resolve().parents[1].joinpath('hardware/bom').glob('*.csv')):
    with path.open(newline='') as stream:
        reader=csv.DictReader(stream)
        rows=list(reader)
        assert {'Ref','Qty','Part','MPN'} <= set(reader.fieldnames or [])
        assert rows and all(row['Qty'].isdigit() for row in rows), path
print('validated BOM files')
