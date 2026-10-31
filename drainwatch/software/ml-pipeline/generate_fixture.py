"""Generate labeled synthetic readings for parser/UI smoke tests; not a training dataset."""
import json, random
rng=random.Random(7)
rows=[{'kind':'inlet','value_mm':max(0,int(rng.gauss(650,80))),'label':'synthetic'} for _ in range(50)]
print(json.dumps(rows))
