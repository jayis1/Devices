"""
AllergySync — API Data Models (Pydantic)
"""

from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime


class SymptomEntry(BaseModel):
    sneezing: int = Field(0, ge=0, le=5)
    itchy_eyes: int = Field(0, ge=0, le=5)
    congestion: int = Field(0, ge=0, le=5)
    runny_nose: int = Field(0, ge=0, le=5)
    headache: int = Field(0, ge=0, le=5)
    fatigue: int = Field(0, ge=0, le=5)
    notes: str = ""
    timestamp: Optional[datetime] = None


class MedicationEntry(BaseModel):
    medication: str
    dose_mg: float
    taken: bool = True
    timestamp: Optional[datetime] = None


class AllergyProfile(BaseModel):
    birch: int = Field(0, ge=0, le=5)
    grass: int = Field(0, ge=0, le=5)
    ragweed: int = Field(0, ge=0, le=5)
    oak: int = Field(0, ge=0, le=5)
    pine: int = Field(0, ge=0, le=5)
    mold: int = Field(0, ge=0, le=5)
    dust_mites: int = Field(0, ge=0, le=5)
    pet_dander: int = Field(0, ge=0, le=5)
    skin_prick_results: dict = Field(default_factory=dict)
    immunotherapy: bool = False


class NodePair(BaseModel):
    node_type: str
    serial: str
    pubkey: str