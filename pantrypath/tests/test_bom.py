from pathlib import Path
import csv

def test_boms_have_headers_and_rows():
    files=list((Path(__file__).parents[1]/"hardware"/"bom").glob("*.csv"))
    assert len(files)==4
    for path in files:
        rows=list(csv.DictReader(path.open(encoding="utf-8")))
        assert rows and {"reference","component","qty","notes"} <= set(rows[0])
