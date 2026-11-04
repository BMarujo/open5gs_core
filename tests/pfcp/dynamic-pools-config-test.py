#!/usr/bin/env python3
"""Actual UPF startup/announcement configuration tests in a disposable namespace.

Usage: UPFD VALID_SMOKE_UPF_YAML OUTPUT. Needs PyYAML, TUN, CAP_NET_ADMIN.
No UE sessions or external services. Artifacts are outside the source checkout.
"""
import copy
import json
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import time
import yaml

binary, fixture, out = map(lambda p: Path(p).resolve(), sys.argv[1:4])
out.mkdir(parents=True, exist_ok=True)
base = yaml.safe_load(fixture.read_text())
results = {}


def ies(data):
    while data:
        typ, n = struct.unpack('!HH', data[:4])
        assert n <= len(data)-4
        yield typ, data[4:4+n]
        data = data[4+n:]


cases = ['missing', 'empty', 'duplicate', 'long', 'invalid-ascii', 'non-ascii',
         'long-dnn', 'invalid-label', 'bad-boolean', 'disabled', 'generic', 'wrong-role',
         'max-id', 'max-groups', 'too-many-groups']
for name in cases:
    config = copy.deepcopy(base)
    sessions = config['upf']['session']
    expected = None
    if name == 'missing': sessions[0].pop('pool_id')
    elif name == 'empty': sessions[0]['pool_id'] = ''
    elif name == 'duplicate': sessions[1]['pool_id'] = sessions[0]['pool_id']
    elif name == 'long': sessions[0]['pool_id'] = 'a'*65
    elif name == 'invalid-ascii': sessions[0]['pool_id'] = 'edge 1'
    elif name == 'non-ascii': sessions[0]['pool_id'] = 'edgé1'
    elif name == 'long-dnn': sessions[0]['dnn'] = 'a'*100
    elif name == 'invalid-label': sessions[0]['dnn'] = '-edge1'
    elif name == 'bad-boolean': config['upf']['pfcp']['advertise_ue_ip_pools'] = 'yes'
    elif name == 'disabled':
        config['upf']['pfcp']['advertise_ue_ip_pools'] = False
        for session in sessions: session.pop('pool_id')
        expected = 0
    elif name == 'generic':
        sessions.append({'subnet': '10.49.0.0/24', 'gateway': '10.49.0.1', 'dev': 'pooltest'})
        expected = 2
    elif name == 'wrong-role':
        config['upf']['pfcp'].pop('advertise_ue_ip_pools')
        config['upf']['pfcp']['learn_upf_dnn'] = True
        expected = 0
    elif name == 'max-id':
        sessions[0]['pool_id'] = 'a'*64
        expected = 2
    elif name in ('max-groups', 'too-many-groups'):
        config['upf']['session'] = [
            {'subnet': '10.%d.0.0/24' % (50+i), 'gateway': '10.%d.0.1' % (50+i),
             'dnn': 'edge%d' % i, 'pool_id': 'edge%d-v4' % i, 'dev': 'pooltest'}
            for i in range(16 if name == 'max-groups' else 17)]
        if name == 'max-groups': expected = 16
    log = out / (name + '.log')
    log.unlink(missing_ok=True)
    config['logger'] = {'level': 'debug', 'file': {'path': str(log)}}
    cfg = out / (name + '.yaml')
    cfg.write_text(yaml.safe_dump(config, sort_keys=False))
    mock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mock.bind(('127.0.0.4', 8805)); mock.settimeout(3)
    with (out / (name + '.console')).open('w') as console:
        proc = subprocess.Popen([str(binary), '-c', str(cfg)], stdout=console, stderr=console)
        try:
            if expected is None:
                assert proc.wait(timeout=5) != 0
                assert 'pfcp_server()' not in log.read_text()
                assert any(text in log.read_text() for text in (
                    'Invalid UPF pool announcement', 'must be true or false',
                    'Too many configured PFCP session subnets'))
            else:
                wire, _ = mock.recvfrom(65535)
                assert wire[1] == 5
                assert sum(typ == 233 for typ, _ in ies(wire[8:])) == expected
                if name == 'generic': assert 'skip generic session without DNN' in log.read_text()
                assert proc.poll() is None
                proc.send_signal(signal.SIGTERM)
                assert proc.wait(timeout=5) == 0
            results[name] = 'PASS'
        finally:
            if proc.poll() is None: proc.kill(); proc.wait()
            mock.close()
(out / 'results.json').write_text(json.dumps(results, indent=2))
print(json.dumps(results, indent=2))
