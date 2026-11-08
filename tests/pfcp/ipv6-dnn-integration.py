#!/usr/bin/env python3
"""Real IPv6-only/dual-stack NAS + dynamic DNN selection; no Kubernetes.

BUILD OUTPUT cp|up. Run using run-ipv6-slice-regressions.sh ... MODE dynamic.
"""
import copy
import ipaddress
import json
import os
from pathlib import Path
import select
import signal
import socket
import struct
import subprocess
import sys
import time
import traceback

import yaml

build, out = (Path(x).resolve() for x in sys.argv[1:3])
mode = sys.argv[3]
assert mode in ('cp', 'up')
out.mkdir(exist_ok=False)
base = yaml.safe_load((build/'configs/slice.yaml').read_text())
base['global'] = {'max': {'ue': 16, 'peer': 16}}
base['db_uri'] = 'mongodb://127.0.0.1/open5gs'
base['test'] = {'serving': [{'plmn_id': {'mcc': 999, 'mnc': 70}}]}
base['amf']['plmn_support'][0]['s_nssai'] = [{'sst': 1}]
base['nssf']['sbi']['client']['nsi'] = base['nssf']['sbi']['client']['nsi'][:1]
base['smf'].pop('freeDiameter', None)
base['smf']['info'] = [{'s_nssai': [{'sst': 1, 'dnn': ['internet', 'edge1']}]}]
base['smf']['pfcp'] = {'server': [{'address': '127.0.0.4'}], 'learn_upf_dnn': True}
if mode == 'cp':
    base['smf']['pfcp']['client'] = {'upf': [
        {'address': '127.0.0.7'}, {'address': '127.0.0.8'}]}
pools = [
    {'subnet': '10.45.0.0/24', 'gateway': '10.45.0.1', 'dnn': 'internet'},
    {'subnet': '10.48.0.0/24', 'gateway': '10.48.0.1', 'dnn': 'edge1'},
    {'subnet': '2001:db8:cafe::/48', 'gateway': '2001:db8:cafe::1', 'dnn': 'edge1'}]
base['smf']['session'] = pools
processes, attempts = [], []
capture = None
pending = b''


def wait(predicate, timeout=20):
    end = time.monotonic()+timeout
    while time.monotonic() < end:
        assert all(p.poll() is None for p in processes), 'NF exited'
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError('condition timeout')


def start(role, name=None, cfg=None):
    name = name or role
    cfg = copy.deepcopy(cfg or base)
    cfg['logger'] = {'level': 'debug', 'file': {'path': str(out/(name+'.log'))}}
    path = out/(name+'.yaml')
    path.write_text(yaml.safe_dump(cfg, sort_keys=False))
    p = subprocess.Popen([str(build/f'src/{role}/open5gs-{role}d'), '-c', str(path)],
        stdout=(out/(name+'.console')).open('w'), stderr=subprocess.STDOUT)
    processes.append(p)
    wait(lambda: 'initialize...done' in (out/(name+'.log')).read_text(errors='replace')
         if (out/(name+'.log')).exists() else False)
    return p


def configure_tun(dev, sessions):
    def attr(kind, data):
        n = 4+len(data)
        return struct.pack('HH', n, kind)+data+b'\0'*((-n)%4)
    index = socket.if_nametoindex(dev)
    updates = [(16, 5, struct.pack('BBHiII', 0, 0, 0, index, 1, 1))]
    for s in sessions:
        family = socket.AF_INET6 if ':' in s['gateway'] else socket.AF_INET
        ip = socket.inet_pton(family, s['gateway'])
        body = struct.pack('BBBBI', family, int(s['subnet'].split('/')[1]), 2, 0, index)
        updates.append((20, 1|4|0x400|0x200, body+attr(1, ip)+attr(2, ip)))
    with socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0) as sock:
        sock.settimeout(3)
        for kind, flags, body in updates:
            sock.sendto(struct.pack('IHHII', 16+len(body), kind, flags, 1, 0)+body, (0, 0))
            assert struct.unpack_from('i', sock.recv(4096), 16)[0] in (0, -17)


def result():
    global pending
    end = time.monotonic()+25
    while time.monotonic() < end:
        if b'\n' not in pending:
            if not select.select([ue.stdout], [], [], .1)[0]:
                assert ue.poll() is None, 'UE exited'
                continue
            raw = os.read(ue.stdout.fileno(), 4096)
            assert raw, 'UE EOF'
            pending += raw
        raw, pending = pending.split(b'\n', 1)
        with (out/'ue-results.log').open('ab') as log:
            log.write(raw+b'\n')
        if raw.startswith(b'RESULT '):
            return json.loads(raw[7:])
    raise AssertionError('UE result timeout')


def command(line):
    ue.stdin.write((line+'\n').encode())
    ue.stdin.flush()
    return result()


status = 1
try:
    with (out/'n2-n4-n3.pcapng').open('wb') as output:
        capture = subprocess.Popen(['tshark', '-i', 'lo', '-f',
            'sctp or udp port 8805 or udp port 2152', '-w', '-'], stdout=output,
            stderr=(out/'capture.log').open('w'))
    wait(lambda: 'Capturing on' in (out/'capture.log').read_text(errors='replace'))
    for role in ['nrf', 'udr', 'udm', 'ausf', 'pcf', 'nssf', 'bsf', 'amf']:
        start(role)
    smf = start('smf')
    for name, addr, sessions in [('a', '127.0.0.7', pools[:1]),
                                 ('b', '127.0.0.8', pools[1:])]:
        sessions = copy.deepcopy(sessions)
        for i, session in enumerate(sessions):
            session.update(dev='v6pool'+name, pool_id=f'v6-{name}-{i}')
        cfg = {'global': base['global'], 'upf': {
            'pfcp': {'server': [{'address': addr}], 'advertise_ue_ip_pools': True},
            'gtpu': {'server': [{'address': addr}]}, 'session': sessions}}
        if mode == 'up':
            cfg['upf']['pfcp']['client'] = {'smf': [{'address': '127.0.0.4'}]}
        start('upf', 'upf-'+name, cfg)
        configure_tun('v6pool'+name, sessions)
    smf_log = out/'smf.log'
    wait(lambda: all(f'PFCP associated [{ip}]' in smf_log.read_text(errors='replace')
                    for ip in ['127.0.0.7', '127.0.0.8']))
    ue_cfg = out/'ue.yaml'
    ue_cfg.write_text(yaml.safe_dump(base))
    ue = subprocess.Popen([str(build/'tests/pfcp/dnn-session-driver'), '-c', str(ue_cfg)],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=(out/'ue.console').open('w'), bufsize=0)
    ready = result()
    assert ready['ready']
    for psi, request, dnn, expected in [(1, 'establish6', 'edge1', '127.0.0.8'),
            (2, 'establish46', 'edge1', '127.0.0.8'),
            (3, 'establish', 'internet', '127.0.0.7')]:
        offset = len(smf_log.read_text(errors='replace'))
        record = {'psi': psi, 'imsi': ready['imsi'], 'request': request,
                  'dnn': dnn, 'start': time.time(), 'expected_first': expected}
        attempts.append(record)
        record.update(command(f'{request} {psi} {dnn}'))
        record['established'] = time.time()
        assert record['accepted'] and record['n3'] == expected, record
        assert record['ipv6'] == (psi != 3), record
        assert record['ipv4'] == (psi != 1), record
        record['selection_logs'] = [x for x in smf_log.read_text(errors='replace')[offset:].splitlines()
                                    if 'UPF selected DNN[' in x]
        assert len(record['selection_logs']) == 1, record
        assert 'source[learned] reason[dnn]' in record['selection_logs'][0], record
        if psi != 3:
            record.update(command(f'rs {psi}'))
            assert ipaddress.ip_address(record['ue_ipv6']) in ipaddress.ip_network(pools[2]['subnet'])
        record['pings'] = []
        gateways = ['2001:db8:cafe::1'] if psi == 1 else [
            '10.48.0.1', '2001:db8:cafe::1'] if psi == 2 else ['10.45.0.1']
        for gateway in gateways:
            for _ in range(3):
                record['pings'].append(dict(command(f'ping {psi} {gateway}'), gateway=gateway))
        record['end'] = time.time()
        (out/'sessions.json').write_text(json.dumps(attempts, indent=2))
    assert smf.poll() is None
    ue.stdin.close()
    assert ue.wait(timeout=10) == 0
    status = 0
except Exception:
    (out/'failure.txt').write_text(traceback.format_exc())
    traceback.print_exc()
finally:
    (out/'sessions.json').write_text(json.dumps(attempts, indent=2))
    if 'ue' in globals() and ue.poll() is None:
        ue.terminate()
        ue.wait(timeout=10)
    if capture:
        # Fast local NAS/data bursts can finish inside libpcap's delivery
        # timeout. Drain capture while NFs are alive before stopping dumpcap.
        time.sleep(2)
        capture.send_signal(signal.SIGINT)
        capture.wait(timeout=10)
    for p in reversed(processes):
        p.terminate()
        p.wait(timeout=10)
    nf_exits = [p.returncode for p in processes]
    if any(code != 0 for code in nf_exits):
        status = 1
    rows = subprocess.check_output(['tshark', '-r', str(out/'n2-n4-n3.pcapng'),
        '-Y', 'pfcp.msg_type == 50', '-T', 'fields', '-e', 'frame.number',
        '-e', 'frame.time_epoch', '-e', 'ip.dst', '-e', 'udp.payload'], text=True)
    (out/'first-n4.tsv').write_text(rows)
    for record in attempts:
        requests = []
        for row in rows.splitlines():
            frame, epoch, dst, payload = row.split('\t')
            if record['start'] <= float(epoch) <= record.get('end', float('inf')):
                wire = bytes.fromhex(payload.replace(':', ''))
                pos = 12 if wire[0]&1 else 4
                requests.append({'frame': int(frame), 'dst': dst,
                                 'sqn': int.from_bytes(wire[pos:pos+3], 'big')})
        record['n4_requests'] = requests
        if status == 0:
            assert len(requests) == 1 and requests[0]['dst'] == record['expected_first'], record
    (out/'sessions.json').write_text(json.dumps(attempts, indent=2))
    (out/'results.json').write_text(json.dumps({'exit': status, 'mode': mode,
        'smf_pid': smf.pid if 'smf' in globals() else None,
        'nf_exit_statuses': nf_exits,
        'scope': 'IPv6-only, dual stack and IPv4 NAS; first N4 and verified GTP gateway echo'}, indent=2))
sys.exit(status)
