#!/usr/bin/env python3
"""Check a commissioned node manifest before local deployment."""
import json, sys
from pathlib import Path
if len(sys.argv) != 2: raise SystemExit('usage: deploy_check.py commissioned-nodes.json')
records=json.loads(Path(sys.argv[1]).read_text())
if not isinstance(records, list) or not records: raise SystemExit('manifest must be a non-empty JSON list')
for record in records:
    if not {'id','type','key_id'} <= record.keys(): raise SystemExit(f'invalid record: {record}')
print(f'{len(records)} commissioned nodes validated; deploy only after physical pairing and accessibility review.')
