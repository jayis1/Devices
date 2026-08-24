from __future__ import annotations

from typing import List, Optional
from sqlmodel import Field, SQLModel


class AllergyProfile(SQLModel, table=True):
    id: Optional[int] = Field(default=None, primary_key=True)
    name: str
    allergens_csv: str
    severity: str = "medium"
    carry_required: bool = True

    @property
    def allergens(self) -> List[str]:
        return [item for item in self.allergens_csv.split(",") if item]


class HouseholdSummary(SQLModel):
    readiness_score: int
    active_alerts: int
    meal_safety_score: int
    carry_compliance_score: int
    temperature_integrity_score: int
