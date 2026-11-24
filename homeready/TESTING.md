# Validation quickstart

From the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -Ihomeready/firmware/common homeready/firmware/common/protocol.c homeready/firmware/common/test_protocol.c -o /tmp/homeready-test && /tmp/homeready-test
python -m unittest discover -s homeready/software/dashboard/tests
python homeready/software/ml-pipeline/train_baseline.py
```

These checks validate the simple C frame-envelope helper, FastAPI prototype routes, and synthetic baseline entry point only. They do not validate node firmware, radio pairing, durable storage, cloud security, ML quality, mobile app, schematics, or hardware.
