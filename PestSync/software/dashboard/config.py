"""
PestSync Backend — Configuration
software/dashboard/config.py
"""
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    # Server
    port: int = 8000
    debug: bool = True

    # Database (TimescaleDB / PostgreSQL)
    database_url: str = "postgresql+asyncpg://pestsync:pestsync@localhost:5432/pestsync"

    # MQTT
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883
    mqtt_user: str = ""
    mqtt_pass: str = ""

    # Firebase (for push notifications)
    firebase_credentials_path: str = ""

    # JWT Auth
    jwt_secret: str = "pestsync-secret-key-change-in-production"
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 10080  # 7 days

    class Config:
        env_file = ".env"


settings = Settings()