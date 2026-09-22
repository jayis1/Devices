# Synthetic smoke baseline authored by jayis1.
import csv, json, random
from pathlib import Path
FEATURES=("lux","dba","co2_ppm","temp_c","rh_pct","eda_us")
def load_rows(path: Path):
    if path.exists():
        with path.open() as f: return [{k:float(v) for k,v in row.items()} for row in csv.DictReader(f)]
    random.seed(7)
    return [{"lux":random.uniform(50,700),"dba":random.uniform(30,70),"co2_ppm":random.uniform(450,1400),"temp_c":22.0,"rh_pct":45.0,"eda_us":random.uniform(1,20)} for _ in range(40)]
def main():
    rows=load_rows(Path("data.csv")); means={key:sum(r[key] for r in rows)/len(rows) for key in FEATURES}
    Path("model-baseline.json").write_text(json.dumps({"author":"jayis1","features":FEATURES,"means":means,"seed":7,"note":"synthetic smoke baseline; not a validated overload model"},indent=2)+"\n")
    print(f"wrote baseline for {len(rows)} synthetic-or-input rows")
if __name__ == "__main__": main()
