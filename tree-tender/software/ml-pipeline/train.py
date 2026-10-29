# Rule baseline only; no trained model/field validation.
import csv
from pathlib import Path
from generate_synthetic import __file__ as generator_file
source=Path(generator_file).with_name('synthetic.csv')
if not source.exists():
    import subprocess, sys
    subprocess.run([sys.executable, str(Path(generator_file))], check=True)
rows=list(csv.DictReader(source.open()))
alerts=sum(int(r['priority']) for r in rows)
print(f'synthetic samples={len(rows)} advisory_priority={alerts}; not a validated predictor')
