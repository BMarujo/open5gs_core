#!/usr/bin/env python3
"""Stage 3 only: run inside a disposable network namespace/container.

Needs PyYAML, tshark, CAP_NET_ADMIN and /dev/net/tun. No NRF, subscribers,
UE sessions, external services or host networking. Usage: BUILD OUTPUT.
Artifacts are written outside the checkout. Throws on any unmet assertion.
"""
import copy
import ipaddress
import json
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import time

import yaml

BUILD, OUT = map(lambda x: Path(x).resolve(), sys.argv[1:3])
OUT.mkdir(parents=True, exist_ok=True)
processes = []
results = {}


def wait_for(predicate, seconds=10):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError('Timed out waiting for test condition')


def start(nf, config, name):
    config = copy.deepcopy(config)
    log = OUT / (name + '.log')
    log.unlink(missing_ok=True)
    config['logger'] = {'level': 'debug', 'file': {'path': str(log)}}
    cfg = OUT / (name + '.yaml')
    cfg.write_text(yaml.safe_dump(config, sort_keys=False))
    stream = (OUT / (name + '.console')).open('w')
    proc = subprocess.Popen([str(BUILD / 'src' / nf / ('open5gs-' + nf + 'd')),
                             '-c', str(cfg)], stdout=stream, stderr=stream)
    processes.append(proc)
    wait_for(lambda: proc.poll() is not None or
             (log.exists() and 'initialize...done' in log.read_text()))
    assert proc.poll() is None, (name, stream.name)
    return proc, log


def stop(proc):
    if proc.poll() is None:
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=10)
    assert proc.returncode == 0, proc.returncode


def ie(typ, value):
    return struct.pack('!HH', typ, len(value)) + value


def packet(typ, xid, body):
    return struct.pack('!BBH', 0x20, typ, len(body) + 4) + xid.to_bytes(3, 'big') + b'\0' + body


def tlvs(data):
    while data:
        typ, n = struct.unpack('!HH', data[:4])
        assert n <= len(data) - 4
        yield typ, data[4:4+n]
        data = data[4+n:]


def pools(wire):
    result = []
    for typ, value in tlvs(wire[8:]):
        if typ != 233:
            continue
        record = {'ids': []}
        for child, val in tlvs(value):
            if child == 177:
                n, = struct.unpack('!H', val[:2])
                assert n == len(val) - 2
                record['ids'].append(val[2:].hex())
            elif child == 22:
                labels = []
                while val:
                    n = val[0]
                    labels.append(val[1:1+n].decode('ascii'))
                    val = val[1+n:]
                record['dnn'] = '.'.join(labels)
            elif child == 258:
                record['family'] = val[0] & 3
        result.append(record)
    return sorted(result, key=lambda x: (x['dnn'], x['family']))


def receive_type(sock, typ, deadline=5):
    end = time.monotonic() + deadline
    sock.settimeout(.2)
    while time.monotonic() < end:
        try:
            wire, peer = sock.recvfrom(65535)
        except socket.timeout:
            continue
        if wire[1] == 1:  # Independently implement heartbeat responder.
            sock.sendto(packet(2, int.from_bytes(wire[4:7], 'big'),
                               ie(96, b'\0\0\0\1')), peer)
        if wire[1] == typ:
            return wire, peer
    raise AssertionError('No PFCP message type %d' % typ)


def send_setup(sock, xid, body, cause):
    sock.sendto(packet(5, xid, body), ('127.0.0.4', 8805))
    wire, _ = receive_type(sock, 6)
    fields = dict(tlvs(wire[8:]))
    assert fields[19] == bytes([cause])
    assert fields[60] == b'\0' + socket.inet_aton('127.0.0.4')
    if cause != 1:
        assert set(fields) == {19, 60}
    return wire


GLOBAL = {'max': {'ue': 16, 'peer': 16}}
SESSIONS = [
    {'subnet': '10.48.0.0/24', 'gateway': '10.48.0.1', 'dnn': 'edge1',
     'pool_id': 'edge1-v4-a', 'dev': 'pooltest'},
    {'subnet': '2001:db8:48::/64', 'gateway': '2001:db8:48::1', 'dnn': 'edge1',
     'pool_id': 'edge1-v6-a', 'dev': 'pooltest'},
]


def upf_config(address='127.0.0.7', active=True, sessions=SESSIONS):
    pfcp = {'advertise_ue_ip_pools': True, 'server': [{'address': address}]}
    if active:
        pfcp['client'] = {'smf': [{'address': '127.0.0.4'}]}
    return {'global': GLOBAL, 'upf': {'pfcp': pfcp,
            'gtpu': {'server': [{'address': address}]}, 'session': sessions}}


def smf_config(active=False, learn=True):
    pfcp = {'learn_upf_dnn': learn, 'server': [{'address': '127.0.0.4'}]}
    if active:
        pfcp['client'] = {'upf': [{'address': '127.0.0.7'}]}
    return {'global': GLOBAL, 'smf': {
        'time': {'message': {'duration': 1000}},  # milliseconds
        'sbi': {'server': [{'address': '127.0.0.4', 'port': 7777}],
                'client': {'nrf': [{'uri': 'http://127.0.0.10:7777'}]}},
        'pfcp': pfcp, 'gtpc': {'server': [{'address': '127.0.0.4'}]},
        'gtpu': {'server': [{'address': '127.0.0.4'}]},
        'session': [{'subnet': '10.48.0.0/24', 'gateway': '10.48.0.1', 'dnn': 'edge1'}],
        'dns': ['8.8.8.8']}}


try:
    capture = subprocess.Popen(['tshark', '-i', 'lo', '-f', 'udp port 8805', '-w',
                                str(OUT / 'association.pcapng')],
                               stdout=subprocess.DEVNULL,
                               stderr=(OUT / 'tshark.log').open('w'))
    time.sleep(.3)
    # Real SMF + real UPF, both initiators. Learning is observed in the real NF
    # event loop after publication, without exposing a production test API.
    for active in (False, True):
        name = 'cp-initiated' if active else 'up-initiated'
        if active:
            up, _ = start('upf', upf_config(active=False), name + '-upf')
            smf, log = start('smf', smf_config(active=True), name + '-smf')
        else:
            smf, log = start('smf', smf_config(), name + '-smf')
            up, _ = start('upf', upf_config(), name + '-upf')
        source = 6 if active else 5
        wait_for(lambda: 'source[%d] status[1] groups[2]' % source in log.read_text())
        assert 'DNN[edge1] family[2]' in log.read_text()
        assert 'DNN[edge1] family[1]' in log.read_text()
        assert 'PFCP associated' in log.read_text()
        assert smf.poll() is None and up.poll() is None
        results[name] = 'PASS'
        if not active:
            # Restart only UPF: the same live SMF learns a fresh replacement.
            smf_pid = smf.pid
            stop(up)
            # A fresh process starts its SQNs at 1. Let the SMF's 750 ms
            # duplicate-request holding window expire before reusing that SQN.
            time.sleep(1)
            up, _ = start('upf', upf_config(), name + '-upf-restarted')
            wait_for(lambda: 'generation[2]' in log.read_text())
            assert smf.pid == smf_pid and smf.poll() is None
            results['same-smf-upf-restart-replacement'] = 'PASS'
        stop(up)
        if not active:
            wait_for(lambda: 'PFCP de-associated' in log.read_text(), 18)
            assert 'PFCP learned pools invalidated' in log.read_text()
            assert 'No Heartbeat from UPF' in log.read_text()
            results['heartbeat-timeout-invalidation'] = 'PASS'
        stop(smf)

    # Negative/error paths through the actual SMF socket callback and FSM.
    mock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mock.bind(('127.0.0.7', 8805))
    smf, log = start('smf', smf_config(), 'incoming-errors-smf')
    node = ie(60, b'\0' + socket.inet_aton('127.0.0.7'))
    base = node + ie(96, b'\0\0\0\1')
    send_setup(mock, 90, node, 66)  # Missing timestamp: no association.
    assert 'PFCP associated' not in log.read_text()
    pool = ie(177, b'\0\x06edge-a') + ie(22, b'\x05edge1') + ie(258, b'\x02')
    send_setup(mock, 91, base + ie(233, b'') + ie(233, pool), 1)
    wait_for(lambda: 'generation[1] source[5] status[2] groups[1]' in log.read_text())
    send_setup(mock, 92, base, 1)
    wait_for(lambda: 'generation[2] source[5] status[0] groups[0]' in log.read_text())
    send_setup(mock, 93, base + ie(233, b'\0'), 68)  # Nested framing defect.
    time.sleep(.05)
    assert 'generation[3]' not in log.read_text()
    send_setup(mock, 94, base + ie(233, pool), 1)
    wait_for(lambda: 'generation[3] source[5] status[1] groups[1]' in log.read_text())
    # Confirm the current advertisement with a real SMF probe. Merely sending
    # an unsolicited Heartbeat must not bind a newly accepted snapshot; queued
    # probes from an earlier Setup generation must not bind it either.
    end = time.monotonic() + 20
    confirmed = 'PFCP learned pools heartbeat confirmed peer[127.0.0.7] generation[3]'
    while confirmed not in log.read_text():
        assert time.monotonic() < end, 'No current-generation Heartbeat confirmation'
        receive_type(mock, 1, deadline=15)
        time.sleep(.05)
    # Advance the confirmed Recovery Timestamp without another Setup/state exit.
    mock.sendto(packet(1, 1001, ie(96, b'\0\0\0\1')), ('127.0.0.4', 8805))
    receive_type(mock, 2)
    mock.sendto(packet(1, 1002, ie(96, b'\0\0\0\2')), ('127.0.0.4', 8805))
    receive_type(mock, 2)
    wait_for(lambda: 'PFCP learned pools invalidated' in log.read_text())
    assert 'PFCP de-associated' not in log.read_text()
    assert smf.poll() is None
    send_setup(mock, 96, base + ie(233, pool), 1)
    wait_for(lambda: 'generation[4] source[5] status[1] groups[1]' in log.read_text())
    results['actual-smf-request-errors-optional-absence-restart'] = 'PASS'
    stop(smf)
    mock.close()

    mock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mock.bind(('127.0.0.7', 8805))
    smf, log = start('smf', smf_config(active=True), 'outgoing-error-smf')
    wire, peer = receive_type(mock, 5)
    xid = int.from_bytes(wire[4:7], 'big')
    # Complete header + Cause + Node ID, but missing mandatory success timestamp.
    mock.sendto(packet(6, xid, node + ie(19, b'\1')), peer)
    wait_for(lambda: 'PFCP Association Setup failed' in log.read_text())
    end = time.monotonic() + 1.5  # > three T1 expirations, < next association attempt.
    mock.settimeout(.1)
    while time.monotonic() < end:
        try:
            reply, _ = mock.recvfrom(65535)
        except socket.timeout:
            continue
        assert not (reply[1] == 5 and int.from_bytes(reply[4:7], 'big') == xid)
    assert 'PFCP associated' not in log.read_text()
    assert 'PFCP learned pools peer' not in log.read_text()
    results['actual-smf-matched-error-stops-retransmission-and-notifies'] = 'PASS'
    stop(smf)
    mock.close()

    # Compare actual UPF-originated wire identities across fresh processes.
    mock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mock.bind(('127.0.0.4', 8805)); mock.settimeout(10)
    observed = []
    for index, (addr, entries) in enumerate([
            ('127.0.0.7', SESSIONS), ('127.0.0.7', SESSIONS),
            ('127.0.0.9', list(reversed(SESSIONS))),
            ('127.0.0.9', [dict(SESSIONS[0], pool_id='edge1-v4-edited'), SESSIONS[1]])]):
        up, _ = start('upf', upf_config(addr, sessions=entries), 'identity-%d' % index)
        while True:
            wire, peer = mock.recvfrom(65535)
            if wire[1] == 5:
                break
        (OUT / ('identity-%d-request.hex' % index)).write_text(wire.hex() + '\n')
        observed.append(pools(wire))
        xid = int.from_bytes(wire[4:7], 'big')
        body = ie(60, b'\0' + socket.inet_aton('127.0.0.4')) + ie(19, b'\1') + ie(96, b'\0\0\0\1')
        mock.sendto(packet(6, xid, body), peer)
        stop(up)
        # Drain queued heartbeat/duplicate datagrams before the next process.
        mock.settimeout(.05)
        try:
            while True: mock.recvfrom(65535)
        except socket.timeout:
            pass
        mock.settimeout(10)
    assert observed[0] == observed[1] == observed[2]
    for records in observed:
        for record in records:
            identity = bytes.fromhex(record['ids'][0]).decode('ascii')
            assert record['family'] == (1 if '-v4-' in identity else 2)
    assert observed[3] != observed[2]
    assert observed[3][1] == observed[2][1]  # V6 (wire bit 2) is second
    results['wire-pool-id-restart-reorder-n4-change-explicit-edit'] = 'PASS'
    (OUT / 'identities.json').write_text(json.dumps(observed, indent=2))
    mock.close()
finally:
    for proc in reversed(processes):
        if proc.poll() is None:
            proc.terminate()
            try: proc.wait(timeout=10)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
    if 'capture' in globals():
        capture.send_signal(signal.SIGINT)
        capture.wait(timeout=10)
    (OUT / 'results.json').write_text(json.dumps(results, indent=2))
print(json.dumps(results, indent=2))
