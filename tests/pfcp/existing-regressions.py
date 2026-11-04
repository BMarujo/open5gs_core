#!/usr/bin/env python3
"""Selected existing 5GC regressions, feature off, in the disposable lab.
BUILD OUTPUT. MongoDB must be local. Each executable owns/cleans its NFs.
"""
import json
import fcntl
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys

import yaml

build, out = (Path(p).resolve() for p in sys.argv[1:3])
out.mkdir(parents=True, exist_ok=False)
results = []
cases = [('attach', 'simple-test')] if len(sys.argv) > 3 and sys.argv[3] == 'epc' else [
    ('registration', 'auth-test'), ('slice', 'different-dnn-test')]
for suite, case in cases:
    cfg = yaml.safe_load((build / ('configs/sample.yaml' if suite == 'registration' else f'configs/{suite}.yaml')).read_text())
    cfg['db_uri'] = 'mongodb://127.0.0.1/open5gs'
    if suite != 'attach':
        cfg['smf'].pop('freeDiameter', None)
    else:
        # EPC regression retains SMF<->PCRF Diameter and SGW-C/SGW-U PFCP.
        for nf in ['nrf', 'scp', 'amf', 'ausf', 'udm', 'pcf', 'nssf', 'bsf', 'udr']:
            cfg['global']['parameter']['no_' + nf] = True
    cfg['logger'] = {'level': 'info'}
    for session in cfg['upf']['session']:
        session['dev'] = 'regtun'
    # Open5GS Linux ogs_tun_set_ip() is a no-op; the test operator owns TUN
    # addresses/up state. Pre-create a persistent TUN inside this lab only.
    with open('/dev/net/tun', 'rb+', buffering=0) as tun:
        fcntl.ioctl(tun.fileno(), 0x400454ca, struct.pack('16sH22x', b'regtun', 0x1001))
        fcntl.ioctl(tun.fileno(), 0x400454cb, 1)
    index = socket.if_nametoindex('regtun')
    def attr(kind, data):
        n = 4 + len(data)
        return struct.pack('HH', n, kind) + data + b'\0' * ((-n) % 4)
    updates = [(16, 5, struct.pack('BBHiII', 0, 0, 0, index, 1, 1))]
    for session in cfg['upf']['session']:
        gateway = session['gateway']
        family = socket.AF_INET6 if ':' in gateway else socket.AF_INET
        prefix = int(session['subnet'].split('/')[1])
        ip = socket.inet_pton(family, gateway)
        body = struct.pack('BBBBI', family, prefix, 2, 0, index) + attr(1, ip) + attr(2, ip)
        updates.append((20, 1 | 4 | 0x400 | 0x200, body))
    with socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0) as sock:
        sock.settimeout(3)
        for typ, flags, payload in updates:
            sock.sendto(struct.pack('IHHII', 16+len(payload), typ, flags, 1, 0) + payload, (0, 0))
            error = struct.unpack_from('i', sock.recv(4096), 16)[0]
            assert error in (0, -17), error
    path = out / (suite + '.yaml')
    path.write_text(yaml.safe_dump(cfg, sort_keys=False))
    command = [str(build / f'tests/{suite}/{suite}'), '-c', str(path), case]
    with (out / (suite + '.log')).open('wb') as log:
        try:
            ret = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode
        except subprocess.TimeoutExpired:
            ret = 124
    results.append({'suite': suite, 'case': case, 'command': command, 'exit': ret})
    (out / 'results.json').write_text(json.dumps(results, indent=2))
    if ret:
        # Timeout cleanup is provided by removing this disposable container;
        # do not run another suite over its possibly surviving child NFs.
        break
sys.exit(int(any(r['exit'] for r in results)))
