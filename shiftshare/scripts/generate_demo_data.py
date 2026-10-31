#!/usr/bin/env python3
"""Generate clearly synthetic examples; writes only beside this script."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).parents[1] / "software/ml-pipeline/train.py"), run_name="__main__")
