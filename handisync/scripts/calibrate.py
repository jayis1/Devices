#!/usr/bin/env python3
import argparse
p=argparse.ArgumentParser(description='Guided low-voltage HandiSync calibration')
p.add_argument('--node',choices=['grip','actuator','dock'],required=True);p.add_argument('--port',required=True)
a=p.parse_args(); print(f'Calibrating {a.node} on {a.port}. Keep mains disconnected; record offsets only after physical checks.')
