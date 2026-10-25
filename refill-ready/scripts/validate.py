import csv, json, pathlib, re
root=pathlib.Path(__file__).resolve().parent.parent
for p in sorted((root/'hardware/bom').glob('*.csv')):
    rows=list(csv.DictReader(p.open()))
    assert rows and all(len(row)==len(rows[0]) for row in rows)
    total=sum(int(r['Qty'])*float(r['Unit_Cost_USD']) for r in rows)
    print(f'{p.name}: {len(rows)} items, ${total:.2f}')
for p in sorted(root.rglob('*.json')):
    json.loads(p.read_text()); print('JSON OK',p.relative_to(root))
for md in [root/'README.md',root.parent/'README.md']:
    for link in re.findall(r'\]\(([^)#]+)',md.read_text()):
        if '://' not in link:
            target=md.parent/link
            assert target.exists(), f'broken link: {md}: {link}'
print('links and formats OK')
