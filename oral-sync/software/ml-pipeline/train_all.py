"""OralSync ML Pipeline — train all 6 models end-to-end."""
import subprocess, sys

MODELS = [
    "train_brushing_technique.py",
    "train_plaque_segmentation.py",
    "train_gingivitis.py",
    "train_caries_detector.py",
    "train_plaque_growth.py",
    "train_caries_risk.py",
]

def main():
    for m in MODELS:
        print(f"\n=== Training {m} ===")
        rc = subprocess.call([sys.executable, m])
        if rc != 0:
            print(f"!! {m} failed (rc={rc})")
            sys.exit(rc)
    print("\n✓ All 6 models trained. Artifacts in ./artifacts/")

if __name__ == "__main__":
    main()