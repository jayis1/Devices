from __future__ import annotations
import json
import os
import sqlite3
from pathlib import Path
from typing import Any
from fastapi import FastAPI, HTTPException, Request

DB_PATH = Path(os.environ.get("BATTERYLOOP_DB", "batteryloop.sqlite3"))
app = FastAPI(title="BatteryLoop prototype", version="0.1.0")


def connect() -> sqlite3.Connection:
    db = sqlite3.connect(DB_PATH)
    db.execute("CREATE TABLE IF NOT EXISTS events (node TEXT NOT NULL, seq INTEGER NOT NULL, kind TEXT NOT NULL, value TEXT NOT NULL, unit TEXT NOT NULL, receipt_ms INTEGER NOT NULL, PRIMARY KEY(node,seq))")
    return db


def validate(e: Any) -> dict:
    if not isinstance(e, dict) or type(e.get("v")) is not int or e.get("v") != 1:
        raise ValueError("unsupported event version")
    node, seq, kind, value, unit = (e.get(k) for k in ("node", "seq", "kind", "value", "unit"))
    if not isinstance(node, str) or not 1 <= len(node) <= 31 or not node.isprintable():
        raise ValueError("invalid node")
    if type(seq) is not int or seq < 0:
        raise ValueError("invalid sequence")
    if kind == "slot_mass_g":
        if unit != "g" or isinstance(value, bool) or not isinstance(value, (int, float)) or not 0 <= value <= 500:
            raise ValueError("invalid mass")
    elif kind in ("bay_present", "return_pressed"):
        if unit != "bool" or type(value) is not bool:
            raise ValueError("invalid boolean event")
    else:
        raise ValueError("unknown kind")
    return {"node": node, "seq": seq, "kind": kind, "value": value, "unit": unit}


@app.post("/events")
async def ingest(request: Request):
    body = await request.body()
    if len(body) > 512:
        raise HTTPException(413, "event too large")
    try:
        e = validate(json.loads(body))
    except (ValueError, json.JSONDecodeError, UnicodeDecodeError) as ex:
        raise HTTPException(422, str(ex)) from ex
    import time
    with connect() as db:
        cur = db.execute("INSERT OR IGNORE INTO events VALUES (?,?,?,?,?,?)", (e["node"], e["seq"], e["kind"], json.dumps(e["value"]), e["unit"], int(time.time() * 1000)))
    return {"accepted": bool(cur.rowcount), "duplicate": cur.rowcount == 0}


@app.get("/events")
def recent(limit: int = 100):
    if not 1 <= limit <= 500:
        raise HTTPException(422, "limit must be 1..500")
    with connect() as db:
        rows = db.execute("SELECT node,seq,kind,value,unit,receipt_ms FROM events ORDER BY receipt_ms DESC LIMIT ?", (limit,)).fetchall()
    return [{"node": n, "seq": s, "kind": k, "value": json.loads(v), "unit": u, "receipt_ms": t} for n,s,k,v,u,t in rows]


@app.get("/health")
def health():
    return {"status": "prototype", "authentication": "not implemented; bind loopback only"}
