#!/usr/bin/env python3
import argparse, json, secrets
p=argparse.ArgumentParser();p.add_argument('--node-id',required=True);p.add_argument('--output',default='provision.json');a=p.parse_args()
record={'node_id':a.node_id,'key_hex':secrets.token_hex(32),'warning':'Transfer through a secure provisioning channel; delete this file after flashing.'}
open(a.output,'w').write(json.dumps(record,indent=2)+'\n');print('wrote',a.output)
