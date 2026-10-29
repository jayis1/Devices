#!/usr/bin/env python3
"""Local smoke test: compile portable protocol codec and execute tests."""
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
out = Path('/tmp/snowshare-protocol-test')
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(root/'firmware/common/protocol.c'),str(root/'firmware/common/test_protocol.c'),'-o',str(out)],check=True)
subprocess.run([str(out)],check=True)
