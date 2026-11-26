from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="CaptionBridge local prototype", version="0.1.0")
segments: list[dict] = []
last_seq = -1

class Caption(BaseModel):
    session: str = Field(min_length=1, max_length=48, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=2**32 - 1)
    text: str = Field(min_length=1, max_length=2000)
    final: bool

@app.get("/health")
def health():
    return {"ok": True, "prototype": True}

@app.post("/sessions/{session}/captions")
def add_caption(session: str, caption: Caption):
    global last_seq
    if caption.session != session:
        raise HTTPException(422, "path and caption session differ")
    if caption.seq <= last_seq:
        raise HTTPException(409, "duplicate or out-of-order caption sequence")
    segments.append(caption.model_dump())
    last_seq = caption.seq
    return {"accepted": True, "seq": caption.seq}

@app.get("/sessions/{session}/captions")
def get_captions(session: str):
    return {"session": session, "captions": [x for x in segments if x["session"] == session]}
