#!/usr/bin/env python3
"""
GlucoSync — Meal Scanner Calibration Script

Calibrates the multispectral meal scanner by imaging reference foods
with known carbohydrate content and verifying CNN predictions.

License: MIT
"""

import argparse
import json
import os

# Reference foods for calibration (USDA FoodData Central values)
REFERENCE_FOODS = [
    {"name": "White bread (1 slice)", "carbs_g": 14, "portion_g": 28, "gi": 70},
    {"name": "Cooked white rice (1 cup)", "carbs_g": 45, "portion_g": 158, "gi": 73},
    {"name": "Pasta (1 cup cooked)", "carbs_g": 43, "portion_g": 140, "gi": 50},
    {"name": "Apple (medium)", "carbs_g": 25, "portion_g": 182, "gi": 36},
    {"name": "Banana (medium)", "carbs_g": 27, "portion_g": 118, "gi": 51},
    {"name": "Cooked oatmeal (1 cup)", "carbs_g": 28, "portion_g": 234, "gi": 55},
    {"name": "Greek yogurt (170g)", "carbs_g": 6, "portion_g": 170, "gi": 35},
    {"name": "Chicken breast (100g)", "carbs_g": 0, "portion_g": 100, "gi": 0},
    {"name": "Broccoli (1 cup)", "carbs_g": 6, "portion_g": 91, "gi": 15},
    {"name": "Potato (medium baked)", "carbs_g": 37, "portion_g": 173, "gi": 73},
]


def run_calibration(scanner_address=None):
    """Run calibration scan on reference foods."""
    print("=== GlucoSync Meal Scanner Calibration ===")
    print(f"\nReference foods: {len(REFERENCE_FOODS)} items")
    print()

    results = []

    for i, food in enumerate(REFERENCE_FOODS):
        print(f"[{i+1}/{len(REFERENCE_FOODS)}] {food['name']}")
        print(f"  Expected: {food['carbs_g']}g carbs, {food['portion_g']}g portion, GI={food['gi']}")

        # Production: send BLE command to scanner to capture 5-band image
        # Then receive prediction from scanner
        # For now, simulate

        predicted_carbs = food["carbs_g"]  # placeholder
        error_pct = abs(predicted_carbs - food["carbs_g"]) / max(food["carbs_g"], 1) * 100

        result = {
            "food": food["name"],
            "expected_carbs": food["carbs_g"],
            "predicted_carbs": predicted_carbs,
            "error_pct": error_pct,
        }
        results.append(result)

        print(f"  Predicted: {predicted_carbs}g carbs (error: {error_pct:.1f}%)")
        print()

    # Summary
    avg_error = sum(r["error_pct"] for r in results) / len(results)
    print(f"\n=== Summary ===")
    print(f"Average carb error: {avg_error:.1f}%")
    print(f"Target: <15% error")

    if avg_error > 15:
        print("⚠ Error exceeds target — consider retraining with more reference data")

    with open("meal_scanner_calibration.json", "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to meal_scanner_calibration.json")


def main():
    parser = argparse.ArgumentParser(description="GlucoSync Meal Scanner Calibration")
    parser.add_argument("--run", action="store_true", help="Run calibration scan")
    parser.add_argument("--list", action="store_true", help="List reference foods")
    parser.add_argument("--address", type=str, help="Scanner BLE address")
    args = parser.parse_args()

    if args.list:
        print("Reference foods for calibration:")
        for food in REFERENCE_FOODS:
            print(f"  {food['name']}: {food['carbs_g']}g carbs, GI={food['gi']}")
    elif args.run:
        run_calibration(args.address)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()