#!/usr/bin/env python3
"""Print a deterministic example event for local API testing."""
import json

print(json.dumps({"v": 1, "event_id": "example-001", "node_id": "station-1",
                  "seq": 1, "observed_at": "2025-01-01T12:00:00Z",
                  "kind": "piano_report", "value": {"status": "ok"}}))
