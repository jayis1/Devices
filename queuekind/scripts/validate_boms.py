import csv
from pathlib import Path
for path in Path("hardware/bom").glob("*.csv"):
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.reader(handle))
    assert rows and rows[0] == ["Ref", "Qty", "Part", "Manufacturer", "MPN", "Description", "Est_Unit_USD"], path
    assert all(len(row) == 7 for row in rows), path
print("validated BOM CSVs")
