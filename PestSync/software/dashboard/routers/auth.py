"""PestSync Backend — Auth Router"""
from fastapi import APIRouter, HTTPException, Depends
from pydantic import BaseModel, EmailStr
from datetime import datetime, timezone, timedelta
import jwt
from config import settings

router = APIRouter()


class LoginRequest(BaseModel):
    email: EmailStr
    password: str


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int


class UserResponse(BaseModel):
    id: int
    email: str
    display_name: str | None


@router.post("/login", response_model=TokenResponse)
async def login(req: LoginRequest):
    # In production: verify against DB with hashed passwords
    if req.email == "demo@pestsync.local" and req.password == "demo":
        expire = datetime.now(timezone.utc) + timedelta(minutes=settings.jwt_expire_minutes)
        token = jwt.encode(
            {"sub": "1", "exp": expire},
            settings.jwt_secret,
            algorithm=settings.jwt_algorithm,
        )
        return TokenResponse(access_token=token, expires_in=settings.jwt_expire_minutes * 60)
    raise HTTPException(status_code=401, detail="Invalid credentials")


@router.post("/register", response_model=UserResponse)
async def register(req: LoginRequest):
    # In production: hash password, store in DB
    return UserResponse(id=1, email=req.email, display_name="PestSync User")


@router.get("/verify", response_model=UserResponse)
async def verify(token: str):
    try:
        payload = jwt.decode(token, settings.jwt_secret, algorithms=[settings.jwt_algorithm])
        return UserResponse(id=int(payload["sub"]), email="demo@pestsync.local",
                            display_name="PestSync User")
    except jwt.PyJWTError:
        raise HTTPException(status_code=401, detail="Invalid token")