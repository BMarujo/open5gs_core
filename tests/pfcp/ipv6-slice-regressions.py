#!/usr/bin/env python3
"""Existing NAS/slice/IPv6 regressions in a disposable Docker network namespace.

BUILD OUTPUT off|cp|up. Use run-ipv6-slice-regressions.sh; Mongo is disposable.
No production behavior or Kubernetes resources are changed.
"""
import fcntl
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
mode = sys.argv[3]
assert mode in ('off', 'cp', 'up')
out.mkdir(exist_ok=False)
results = []


def setup_tun(sessions):
    with open('/dev/net/tun', 'rb+', buffering=0) as tun:
        fcntl.ioctl(tun.fileno(), 0x400454ca,
                    struct.pack('16sH22x', b'v6regtun', 0x1001))
        fcntl.ioctl(tun.fileno(), 0x400454cb, 1)
    index = socket.if_nametoindex('v6regtun')

    def attr(kind, data):
        n = 4 + len(data)
        return struct.pack('HH', n, kind) + data + b'\0' * ((-n) % 4)

    updates = [(16, 5, struct.pack('BBHiII', 0, 0, 0, index, 1, 1))]
    for session in sessions:
        family = socket.AF_INET6 if ':' in session['gateway'] else socket.AF_INET
        ip = socket.inet_pton(family, session['gateway'])
        body = struct.pack('BBBBI', family,
                int(session['subnet'].split('/')[1]), 2, 0, index)
        body += attr(1, ip) + attr(2, ip)
        updates.append((20, 1 | 4 | 0x400 | 0x200, body))
    with socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0) as sock:
        sock.settimeout(3)
        for typ, flags, payload in updates:
            sock.sendto(struct.pack('IHHII', 16+len(payload), typ, flags, 1, 0)
                        + payload, (0, 0))
            error = struct.unpack_from('i', sock.recv(4096), 16)[0]
            assert error in (0, -17), error


cases = [('registration', 'simple-test'), ('slice', 'same-dnn-test'),
         ('slice', 'different-dnn-test')]
if mode == 'off':
    cases.append(('attach', 'simple-test'))
for suite, case in cases:
    name = suite + '-' + case
    cfg = yaml.safe_load((build / ('configs/sample.yaml' if suite == 'registration'
                                   else f'configs/{suite}.yaml')).read_text())
    cfg['db_uri'] = 'mongodb://127.0.0.1/open5gs'
    if suite != 'attach':
        cfg['smf'].pop('freeDiameter', None)
    else:
        for nf in ['nrf', 'scp', 'amf', 'ausf', 'udm', 'pcf', 'nssf', 'bsf', 'udr']:
            cfg['global']['parameter']['no_' + nf] = True
    cfg['logger'] = {'level': 'info'}
    cfg['smf']['pfcp']['learn_upf_dnn'] = mode != 'off'
    cfg['upf']['pfcp']['advertise_ue_ip_pools'] = mode != 'off'
    if mode != 'off':
        for i, session in enumerate(cfg['upf']['session']):
            session.update(dnn='internet', pool_id='regression-' + str(i))
        if mode == 'up':
            cfg['smf']['pfcp'].pop('client', None)
            cfg['upf']['pfcp']['client'] = {'smf': [{'address': '127.0.0.4'}]}
        else:
            cfg['smf']['pfcp']['client'] = {'upf': [{'address': '127.0.0.7'}]}
    for session in cfg['upf']['session']:
        session['dev'] = 'v6regtun'
    setup_tun(cfg['upf']['session'])
    path = out / (name + '.yaml')
    path.write_text(yaml.safe_dump(cfg, sort_keys=False))
    with (out / (name + '.pcapng')).open('wb') as capture_output:
        capture = subprocess.Popen(['tshark', '-i', 'lo', '-f',
            'sctp or udp port 8805 or udp port 2152', '-w', '-'],
            stdout=capture_output, stderr=(out / (name + '-capture.log')).open('w'))
    deadline = time.monotonic()+15
    while 'Capturing on' not in (out/(name+'-capture.log')).read_text(errors='replace'):
        assert capture.poll() is None and time.monotonic() < deadline
        time.sleep(.05)
    # ABTS forces ERROR unless -e is passed; YAML logger.level is overridden.
    command = [str(build / f'tests/{suite}/{suite}'), '-c', str(path),
               '-e', 'info', case]
    with (out / (name + '.log')).open('wb') as log:
        try:
            ret = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                 timeout=150).returncode
        except subprocess.TimeoutExpired:
            ret = 124
    capture.send_signal(signal.SIGINT)
    capture.wait(timeout=10)
    record = {'suite': suite, 'case': case, 'mode': mode, 'command': command,
              'exit': ret, 'capture_exit': capture.returncode}
    # The existing C tests own the NFs and verify NAS/slice outcomes; capture
    # independently verifies real bidirectional IPv6/RS/RA where exercised.
    counts = {}
    for label, expression in {
        'ipv6_echo_request': 'gtp && icmpv6.type == 128',
        'ipv6_echo_reply': 'gtp && icmpv6.type == 129',
        'router_solicitation': 'gtp && icmpv6.type == 133',
        'router_advertisement': 'gtp && icmpv6.type == 134',
        'n4_session_request': 'pfcp.msg_type == 50',
    }.items():
        rows = subprocess.check_output(['tshark', '-r', str(out/(name+'.pcapng')),
                    '-Y', expression, '-T', 'fields', '-e', 'frame.number'], text=True)
        counts[label] = len(rows.splitlines())
    record['packet_counts'] = counts
    if ret == 0 and suite in ('registration', 'attach'):
        record['ipv6_data_verified'] = all(counts[x] > 0 for x in
            ['ipv6_echo_request', 'ipv6_echo_reply', 'router_solicitation',
             'router_advertisement'])
    if ret == 0 and mode != 'off':
        log = (out/(name+'.log')).read_text(errors='replace')
        record['learned_dnn_selection'] = 'source[learned] reason[dnn]' in log
    results.append(record)
    (out/'results.json').write_text(json.dumps(results, indent=2))
    print(json.dumps(record), flush=True)
    if ret != 0 or capture.returncode != 0:
        break  # Container removal handles any children left by a timed-out test.
sys.exit(int(any(r['exit'] != 0 or r['capture_exit'] != 0 or
                 r.get('ipv6_data_verified') is False or
                 r.get('learned_dnn_selection') is False for r in results)))
