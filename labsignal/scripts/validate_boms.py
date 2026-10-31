import csv
from pathlib import Path
for path in sorted(Path('hardware/bom').glob('*.csv')):
    with path.open(newline='') as stream:
        rows = list(csv.reader(stream))
    assert rows and all(len(row) == len(rows[0]) for row in rows), path
    print(f'{path}: {len(rows)-1} BOM rows; columns consistent')
