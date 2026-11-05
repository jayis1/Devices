from fastapi import FastAPI, HTTPException, Query
from pydantic import BaseModel, Field
import sqlite3
from pathlib import Path
DB = Path(__file__).with_name("movemate.sqlite3")
app = FastAPI(title="MoveMate local inventory API", version="1.0")
class Box(BaseModel):
    box_id: str = Field(min_length=8, max_length=64)
    contents: str = Field(min_length=1, max_length=500)
    room: str = Field(default="", max_length=80)
def connect():
    c = sqlite3.connect(DB)
    c.execute("CREATE TABLE IF NOT EXISTS boxes (box_id TEXT PRIMARY KEY, contents TEXT NOT NULL, room TEXT NOT NULL)")
    return c
@app.put("/v1/boxes/{box_id}")
def put_box(box_id: str, box: Box):
    if box.box_id != box_id: raise HTTPException(400, "path and body box_id differ")
    with connect() as c: c.execute("INSERT INTO boxes VALUES (?, ?, ?) ON CONFLICT(box_id) DO UPDATE SET contents=excluded.contents, room=excluded.room", (box.box_id, box.contents, box.room))
    return {"box_id": box.box_id, "contents": box.contents, "room": box.room}
@app.get("/v1/search")
def search(q: str = Query(min_length=1, max_length=120)):
    with connect() as c: rows = c.execute("SELECT box_id, contents, room FROM boxes WHERE contents LIKE ? ORDER BY box_id LIMIT 50", (f"%{q}%",)).fetchall()
    return {"results": [{"box_id": r[0], "contents": r[1], "room": r[2]} for r in rows]}
