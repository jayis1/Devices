import json
from pathlib import Path

def main():
    # Transparent, deterministic synthetic demonstration; no real performance claim.
    rows=[{'occupancy':i%2,'tool_present':(i//2)%2,'ready':int(i%2==0)} for i in range(100)]
    split=80
    model={'features':['occupancy','tool_present'],'rule':'ready = 1 - occupancy','train_rows':split,'evaluation_rows':len(rows)-split,'synthetic':True}
    assert all(r['ready']==1-r['occupancy'] for r in rows)
    Path('synthetic_baseline.json').write_text(json.dumps(model, indent=2)+'\n')
    print('synthetic baseline rule verified; no efficacy claim')

if __name__=='__main__': main()
