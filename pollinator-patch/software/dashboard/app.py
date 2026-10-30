from __future__ import annotations
import sqlite3
from contextlib import closing
from pathlib import Path
from typing import Literal
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
DB=Path(__file__).with_name('pollinator.db')
app=FastAPI(title='PollinatorPatch demo',version='0.1.0')
class Observation(BaseModel):
    bed_id:int=Field(ge=0,le=255)
    kind:Literal['bed_sample','bloom']
    value:str=Field(min_length=1,max_length=160)
    observed_at:int=Field(ge=0)
def connect():
    db=sqlite3.connect(DB); db.execute('CREATE TABLE IF NOT EXISTS observations(id INTEGER PRIMARY KEY,bed_id INTEGER,kind TEXT,value TEXT,observed_at INTEGER)'); return db
@app.post('/observations',status_code=201)
def create(item:Observation):
    with closing(connect()) as db:
        cur=db.execute('INSERT INTO observations(bed_id,kind,value,observed_at) VALUES(?,?,?,?)',(item.bed_id,item.kind,item.value,item.observed_at)); db.commit(); return {'id':cur.lastrowid}
@app.get('/observations')
def list_observations(limit:int=100):
    if not 1<=limit<=500: raise HTTPException(400,'limit must be 1..500')
    with closing(connect()) as db:
        return [{'id':r[0],'bed_id':r[1],'kind':r[2],'value':r[3],'observed_at':r[4]} for r in db.execute('SELECT id,bed_id,kind,value,observed_at FROM observations ORDER BY id DESC LIMIT ?',(limit,))]
