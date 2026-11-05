from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3, json, os
DB=os.getenv("FIXFORWARD_DB","fixforward.sqlite3")
app=FastAPI(title="FixForward local event API",version="1.0")
def init():
 with sqlite3.connect(DB) as c:c.execute("create table if not exists events(id text primary key, category text, weight_g int, route text, created text default current_timestamp)")
class Intake(BaseModel):
 id:str=Field(min_length=1,max_length=64)
 category:str=Field(min_length=1,max_length=40)
 weight_g:int=Field(ge=0,le=100000)
 route:str=Field(pattern="^(reuse|repair|battery_review|recycler)$")
@app.on_event("startup")
def startup():init()
@app.post("/v1/intake")
def intake(x:Intake):
 try:
  with sqlite3.connect(DB) as c:c.execute("insert into events(id,category,weight_g,route) values(?,?,?,?)",(x.id,x.category,x.weight_g,x.route))
 except sqlite3.IntegrityError:raise HTTPException(409,"duplicate event id")
 return {"accepted":True,"id":x.id}
@app.get("/v1/events")
def events():
 with sqlite3.connect(DB) as c:return [{"id":r[0],"category":r[1],"weight_g":r[2],"route":r[3]} for r in c.execute("select id,category,weight_g,route from events order by created")]
