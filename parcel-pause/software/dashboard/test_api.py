import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dashboard'))
from main import Reading, ingest

def test_schema_bounds():
    assert Reading(node_id=1,node_type=1,sequence=0,age_ms=0,value=0,status=0).value == 0
    try: Reading(node_id=0,node_type=1,sequence=0,age_ms=0,value=0,status=0)
    except Exception: return
    raise AssertionError('invalid id accepted')
