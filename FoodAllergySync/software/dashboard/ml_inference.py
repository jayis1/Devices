from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable


@dataclass
class LabelAssessment:
    risk_level: str
    allergens: list[str]
    rationale: str


RISK_TOKENS = {
    "peanut": "peanut",
    "tree nut": "tree_nut",
    "milk": "milk",
    "egg": "egg",
    "sesame": "sesame",
    "shared equipment": "cross_contact",
    "may contain": "cross_contact",
}


def assess_label(text: str, household_allergens: Iterable[str]) -> LabelAssessment:
    normalized = text.lower()
    hits = [name for token, name in RISK_TOKENS.items() if token in normalized]
    household = set(household_allergens)
    direct_hits = [hit for hit in hits if hit in household]
    if direct_hits:
        return LabelAssessment("unsafe", direct_hits, "Direct allergen match detected")
    if "cross_contact" in hits:
        return LabelAssessment("caution", hits, "Cross-contact phrase detected")
    return LabelAssessment("safe", [], "No known risk tokens detected")


def summarize_strip_ratio(ratio: float) -> str:
    if ratio >= 0.55:
        return "positive"
    if ratio >= 0.30:
        return "trace"
    if ratio > 0.05:
        return "negative"
    return "invalid"
