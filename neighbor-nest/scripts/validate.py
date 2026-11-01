import csv, glob, json, py_compile, subprocess
for path in ['software/dashboard/backend/app.py','software/ml-pipeline/train.py','scripts/calibrate.py']:
    py_compile.compile(path, doraise=True)
with open('software/mobile-app/package.json') as f: json.load(f)
for path in glob.glob('hardware/bom/*.csv'):
    with open(path, newline='') as f:
        rows=list(csv.reader(f))
    assert len({len(row) for row in rows}) == 1, path
    assert rows[0][0] == 'Ref', path
    print(f'{path}: {len(rows)-1} rows, consistent columns')
subprocess.run(['python3','software/ml-pipeline/train.py'], check=True)
print('Python syntax, package JSON, BOM parsing, and synthetic ML demo passed')
