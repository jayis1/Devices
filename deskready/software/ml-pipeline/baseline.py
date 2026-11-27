"""Synthetic-only baseline: classify freshness; no trained model or performance claim."""
from datetime import datetime, timezone, timedelta

def band(last_seen, now=None):
    now=now or datetime.now(timezone.utc)
    age=(now-last_seen).total_seconds()
    return 'current' if age<=600 else 'stale'
if __name__=='__main__':
    now=datetime.now(timezone.utc)
    assert band(now,now)=='current'
    assert band(now-timedelta(seconds=601),now)=='stale'
    print('synthetic freshness baseline smoke test passed')
