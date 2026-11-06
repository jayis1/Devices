from __future__ import annotations
from fastapi import FastAPI, HTTPException, Request
from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator
from typing import Any

app = FastAPI(title="PowerBankLoop prototype API", version="0.1.0")
class Event(BaseModel):
    model_config = ConfigDict(extra="forbid")
    v: int = Field(ge=1, le=1)
    site: str = Field(min_length=1, max_length=32, pattern=r"^[A-Za-z0-9_-]+$")
    node: str = Field(min_length=1, max_length=32, pattern=r"^[A-Za-z0-9_-]+$")
    seq: int = Field(ge=0, le=4294967295)
    ts: int | None = Field(default=None, ge=0)
    kind: str
    payload: dict[str, Any]
    @field_validator("kind")
    @classmethod
    def allowed_kind(cls, value: str) -> str:
        if value not in {"DOCK_STATUS", "PACK_STATUS", "HEARTBEAT"}:
            raise ValueError("unsupported kind")
        return value
    @model_validator(mode="after")
    def validate_payload(self):
        p=self.payload
        if self.kind=="DOCK_STATUS":
            if set(p)!={"occupied","total","ready","faults"} or any(type(p[k]) is not int for k in p):
                raise ValueError("invalid dock payload")
            if not 1<=p["total"]<=32 or not 0<=p["ready"]<=p["occupied"]<=p["total"] or not 0<=p["faults"]<=p["total"]:
                raise ValueError("dock counts out of range")
        elif self.kind=="PACK_STATUS":
            if set(p)!={"pack_id","soc_bucket","temp_deci_c"} or not isinstance(p["pack_id"],str) or not 1<=len(p["pack_id"])<=32:
                raise ValueError("invalid pack payload")
            if type(p["soc_bucket"]) is not int or not 0<=p["soc_bucket"]<=4 or type(p["temp_deci_c"]) is not int or not -200<=p["temp_deci_c"]<=800:
                raise ValueError("pack values out of range")
        elif set(p)!={"uptime_s","dropped"} or any(type(v) is not int or v<0 for v in p.values()):
            raise ValueError("invalid heartbeat payload")
        return self

latest: dict[tuple[str,str], Event] = {}
last_seq: dict[tuple[str,str], int] = {}
@app.get("/health")
def health(): return {"status":"ok"}
@app.post("/v1/events", status_code=202)
async def ingest(event: Event, request: Request):
    if len(await request.body()) > 4096:
        raise HTTPException(413, "event too large")
    key=(event.site,event.node)
    previous=last_seq.get(key)
    if previous is not None and event.seq<=previous: return {"accepted":False,"reason":"duplicate_or_stale"}
    last_seq[key]=event.seq; latest[key]=event
    return {"accepted":True}
@app.get("/v1/sites/{site}/latest")
def site_latest(site: str):
    return {"events":[e.model_dump() for (s,_),e in latest.items() if s==site]}
