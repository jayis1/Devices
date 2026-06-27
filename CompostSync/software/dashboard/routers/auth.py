"""Auth router — OAuth2/JWT for user authentication."""
from datetime import datetime, timedelta
from fastapi import APIRouter, Depends, HTTPException, status
from jose import jwt, JWTError
from pydantic import BaseModel

from config import settings
from db import get_db

router = APIRouter()


class LoginRequest(BaseModel):
    email: str
    password: str


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int


class RegisterRequest(BaseModel):
    email: str
    name: str
    password: str


def create_access_token(data: dict) -> str:
    to_encode = data.copy()
    expire = datetime.utcnow() + timedelta(hours=settings.jwt_expire_hours)
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, settings.jwt_secret, algorithm=settings.jwt_algorithm)


async def get_current_user(token: str = Depends(None)):
    # Simplified — implement OAuth2 password bearer in production
    pass


@router.post("/register", response_model=TokenResponse)
async def register(req: RegisterRequest, db=Depends(get_db)):
    """Register a new user."""
    # Check if email exists, hash password, create user, return JWT
    token = create_access_token({"sub": req.email})
    return TokenResponse(access_token=token)


@router.post("/login", response_model=TokenResponse)
async def login(req: LoginRequest, db=Depends(get_db)):
    """Login and get JWT token."""
    # Verify credentials, return JWT
    token = create_access_token({"sub": req.email})
    return TokenResponse(access_token=token)