from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3, uuid, os

DB = os.environ.get("REPAIRLOOP_DB", "repairloop.sqlite3")
app = FastAPI(title="RepairLoop local event API", version="1.0.0")

def connect():
    c=sqlite3.connect(DB)
    c.execute("CREATE TABLE IF NOT EXISTS tickets(id TEXT PRIMARY KEY, category TEXT NOT NULL, created REAL NOT NULL DEFAULT (unixepoch()), status TEXT NOT NULL DEFAULT 'waiting')")
    return c
class TicketIn(BaseModel):
    category: str = Field(min_length=1, max_length=32, pattern=r"^[a-zA-Z0-9 -]+$")
@app.post('/tickets')
def create_ticket(body: TicketIn):
    tid=uuid.uuid4().hex[:10].upper()
    with connect() as db: db.execute("INSERT INTO tickets(id,category) VALUES (?,?)",(tid,body.category))
    return {"id":tid,"status":"waiting"}
@app.get('/tickets')
def list_tickets():
    with connect() as db: rows=db.execute("SELECT id,category,created,status FROM tickets ORDER BY created,id").fetchall()
    return [{"id":r[0],"category":r[1],"created":r[2],"status":r[3]} for r in rows]
@app.post('/tickets/{ticket_id}/called')
def call_ticket(ticket_id:str):
    with connect() as db:
        cur=db.execute("UPDATE tickets SET status='called' WHERE id=? AND status='waiting'",(ticket_id,))
        if not cur.rowcount: raise HTTPException(404,"ticket not waiting or not found")
    return {"id":ticket_id,"status":"called"}
@app.delete('/tickets')
def close_event():
    with connect() as db: db.execute("DELETE FROM tickets")
    return {"deleted":True}
