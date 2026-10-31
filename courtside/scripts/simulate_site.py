import json
from datetime import datetime, timezone
from urllib.request import Request, urlopen

BASE = "http://127.0.0.1:8000/observations"


def sample(seq: int = 1) -> dict:
    return {"version": 1, "site_id": "park-17", "node_id": "surface-a", "seq": seq,
            "observed_at": datetime.now(timezone.utc).isoformat(), "kind": "surface_observation",
            "data": {"wetness_raw": 812, "temperature_c": 17.4, "humidity_pct": 61.0}}

if __name__ == "__main__":
    req = Request(BASE, data=json.dumps(sample()).encode(), headers={"Content-Type": "application/json"}, method="POST")
    with urlopen(req, timeout=3) as response:
        print(response.status, response.read().decode())
