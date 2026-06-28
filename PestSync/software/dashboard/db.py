"""
PestSync Backend — Database Connection
software/dashboard/db.py
"""
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession, async_sessionmaker
from sqlalchemy.orm import declarative_base
from config import settings

engine = create_async_engine(settings.database_url, echo=settings.debug, pool_size=10)
async_session_factory = async_sessionmaker(engine, class_=AsyncSession, expire_on_commit=False)

Base = declarative_base()


class Database:
    def __init__(self):
        self._connected = False

    @property
    def is_connected(self):
        return self._connected

    async def connect(self):
        async with engine.begin() as conn:
            await conn.run_sync(Base.metadata.create_all)
        self._connected = True

    async def disconnect(self):
        await engine.dispose()
        self._connected = False

    async def get_session(self) -> AsyncSession:
        async with async_session_factory() as session:
            yield session


database = Database()