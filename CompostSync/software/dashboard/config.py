"""
CompostSync Backend — Configuration
"""
import os
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    # Database
    database_url: str = os.getenv("DATABASE_URL", "postgresql://compost:compost@localhost/compostsync")

    # MQTT
    mqtt_host: str = os.getenv("MQTT_HOST", "localhost")
    mqtt_port: int = int(os.getenv("MQTT_PORT", "1883"))
    mqtt_user: str = os.getenv("MQTT_USER", "")
    mqtt_pass: str = os.getenv("MQTT_PASS", "")

    # Firebase (for push notifications)
    fcm_server_key: str = os.getenv("FCM_SERVER_KEY", "")

    # ML model paths
    model_dir: str = os.getenv("MODEL_DIR", "/app/models")

    # JWT
    jwt_secret: str = os.getenv("JWT_SECRET", "compostsync-dev-secret-change-me")
    jwt_algorithm: str = "HS256"
    jwt_expire_hours: int = 24

    class Config:
        env_file = ".env"


settings = Settings()