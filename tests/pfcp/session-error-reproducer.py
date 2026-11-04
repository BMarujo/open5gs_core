#!/usr/bin/env python3
"""Characterize the independent Session Establishment error Node ID defect.

BUILD OUTPUT. Run in an isolated --network none container with TUN/NET_ADMIN.
Independent wire request omits mandatory CP F-SEID after a valid association.
This deliberately expects the known defect; it does not weaken the SMF parser.
"""
import json
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import time

import yaml

build, out = (Path(p).resolve() for p in sys.argv[1:3])
out.mkdir(parents=True, exist_ok=False)
cfg = {'global': {'max': {'ue': 16, 'peer': 16}},
       'logger': {'level': 'debug', 'file': {'path': str(out / 'upf.log')}},
       'upf': {'pfcp': {'server': [{'address': '127.0.0.7'}]},
               'gtpu': {'server': [{'address': '127.0.0.7'}]},
               'session': [{'subnet': '10.45.0.0/24', 'gateway': '10.45.0.1',
                            'dnn': 'internet', 'dev': 'reprotun'}]}}
(out / 'upf.yaml').write_text(yaml.safe_dump(cfg))


def ie(typ, value):
    return struct.pack('!HH', typ, len(value)) + value


def packet(typ, sqn, body, session=False):
    return (struct.pack('!BBH', 0x21 if session else 0x20, typ, len(body) + (12 if session else 4))
            + (b'\0' * 8 if session else b'') + sqn.to_bytes(3, 'big') + b'\0' + body)


capture = subprocess.Popen(['tshark', '-i', 'lo', '-f', 'udp port 8805',
            '-w', str(out / 'repro.pcapng')], stdout=subprocess.DEVNULL,
            stderr=(out / 'capture.log').open('w'))
proc = subprocess.Popen([str(build / 'src/upf/open5gs-upfd'), '-c', str(out / 'upf.yaml')],
            stdout=(out / 'upf.console').open('w'), stderr=subprocess.STDOUT)
try:
    time.sleep(1)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.4', 8805)); sock.settimeout(5)
        node = ie(60, b'\0' + socket.inet_aton('127.0.0.4'))
        sock.sendto(packet(5, 1, node + ie(96, struct.pack('!I', 123456))), ('127.0.0.7', 8805))
        response = sock.recv(8192)
        assert response[1] == 6, response.hex()
        sock.sendto(packet(50, 2, node, True), ('127.0.0.7', 8805))
        while True:
            response = sock.recv(8192)
            if response[1] == 51:
                break
            assert response[1] == 1, response.hex()
            sqn = int.from_bytes(response[4:7], 'big')
            sock.sendto(packet(2, sqn, ie(96, struct.pack('!I', 123456))), ('127.0.0.7', 8805))
        fields, pos = {}, 16 if response[0] & 1 else 8
        while pos < len(response):
            typ, size = struct.unpack_from('!HH', response, pos)
            fields[typ] = response[pos+4:pos+4+size].hex(); pos += 4+size
        result = {'response_hex': response.hex(), 'ies': fields,
                  'cause': int(fields[19], 16), 'node_id_present': 60 in fields}
        assert result['cause'] == 66 and not result['node_id_present'], result
        result['characterization'] = 'PASS (known protocol defect reproduced, not corrected)'
        (out / 'result.json').write_text(json.dumps(result, indent=2))
finally:
    capture.send_signal(signal.SIGINT); capture.wait(timeout=10)
    proc.terminate(); proc.wait(timeout=10)
