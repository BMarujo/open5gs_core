#!/usr/bin/env python3
"""Stage 4A: isolated PFCP + actual SMF allocation/selection boundary.

Usage: BUILD OUTPUT. Run in a disposable --network none container with
NET_ADMIN and /dev/net/tun. Needs PyYAML and tshark. The test-only SMF driver
injects allocated session objects on the NF poll thread. It does not emulate
NAS/UDM/PCF or claim UE/N6/PFCP session-establishment interoperability.
"""
import copy
import json
import os
from pathlib import Path
import queue
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

import yaml

BUILD, OUT = (Path(p).resolve() for p in sys.argv[1:3])
OUT.mkdir(parents=True, exist_ok=True)
processes, mocks, observations, results = [], [], [], {}
GLOBAL = {'max': {'ue': 16, 'peer': 16}}
SUBNETS = {'internet': ('10.45.0.0/24', '10.45.0.1'),
           'edge1': ('10.48.0.0/24', '10.48.0.1'),
           'other': ('10.49.0.0/24', '10.49.0.1')}


def wait_for(predicate, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError('Timed out waiting for test condition')


def smf_config(learn=True, clients=None):
    pfcp = {'learn_upf_dnn': learn, 'server': [{'address': '127.0.0.4'}]}
    if clients:
        pfcp['client'] = {'upf': clients}
    sessions = [{'subnet': subnet, 'gateway': gateway, 'dnn': dnn}
                for dnn, (subnet, gateway) in SUBNETS.items()]
    sessions += [{'subnet': '2001:db8:48::/48', 'gateway': '2001:db8:48::1', 'dnn': 'edge1'},
                 {'subnet': '10.50.0.0/24', 'gateway': '10.50.0.1'}]
    return {'global': GLOBAL, 'smf': {
        'time': {'message': {'duration': 1000}},
        'sbi': {'server': [{'address': '127.0.0.4', 'port': 7777}],
                'client': {'nrf': [{'uri': 'http://127.0.0.10:7777'}]}},
        'pfcp': pfcp, 'gtpc': {'server': [{'address': '127.0.0.4'}]},
        'gtpu': {'server': [{'address': '127.0.0.4'}]},
        'session': sessions, 'dns': ['8.8.8.8']}}


def upf_config(address, dnn, dev):
    subnet, gateway = SUBNETS[dnn]
    return {'global': GLOBAL, 'upf': {
        'pfcp': {'advertise_ue_ip_pools': True, 'server': [{'address': address}],
                 'client': {'smf': [{'address': '127.0.0.4'}]}},
        'gtpu': {'server': [{'address': address}]},
        'session': [{'subnet': subnet, 'gateway': gateway, 'dnn': dnn,
                     'pool_id': dnn + '-' + dev, 'dev': dev}]}}


class NF:
    def __init__(self, name, config, driver=False):
        self.name, self.driver = name, driver
        self.log = OUT / (name + '.log')
        self.console = (OUT / (name + '.console')).open('w')
        self.log.unlink(missing_ok=True)
        config = copy.deepcopy(config)
        config['logger'] = {'level': 'debug', 'file': {'path': str(self.log)}}
        cfg = OUT / (name + '.yaml')
        cfg.write_text(yaml.safe_dump(config, sort_keys=False))
        binary = BUILD / ('tests/unit/smf-selector-driver' if driver else 'src/upf/open5gs-upfd')
        self.proc = subprocess.Popen([str(binary), '-c', str(cfg)],
                    stdin=subprocess.PIPE if driver else subprocess.DEVNULL,
                    stdout=subprocess.PIPE if driver else self.console,
                    stderr=self.console, bufsize=0)
        self.pending = b''
        processes.append(self)
        wait_for(lambda: self.proc.poll() is not None or
                 (self.log.exists() and 'initialize...done' in self.text()))
        assert self.proc.poll() is None, name

    def text(self):
        return self.log.read_text() if self.log.exists() else ''

    def command(self, line, expected=None, reason=None, source=None):
        offset = len(self.text())
        self.proc.stdin.write((line + '\n').encode())
        end = time.monotonic() + 8
        while time.monotonic() < end:
            if b'\n' not in self.pending:
                if not select.select([self.proc.stdout], [], [], .1)[0]:
                    assert self.proc.poll() is None, self.name
                    continue
                chunk = os.read(self.proc.stdout.fileno(), 4096)
                assert chunk, self.name
                self.pending += chunk
            raw, self.pending = self.pending.split(b'\n', 1)
            self.console.write(raw.decode(errors='replace') + '\n'); self.console.flush()
            if not raw.startswith(b'RESULT '):
                continue
            record = json.loads(raw[7:])
            record.update(test=self.name, command=line)
            if expected:
                assert expected in record['selected'], record
            if line.startswith('select '):
                matches = re.findall(r'UPF selected DNN\[.*', self.text()[offset:])
                assert len(matches) == 1, matches
                record['selection_log'] = matches[0]
                if reason: assert 'reason[%s]' % reason in matches[0], record
                if source: assert 'source[%s]' % source in matches[0], record
            observations.append(record)
            return record
        raise AssertionError('No driver result: ' + line)

    def stop(self):
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGCONT)
            self.proc.terminate(); self.proc.wait(timeout=10)
        assert self.proc.returncode == 0, (self.name, self.proc.returncode)


def ie(typ, value):
    return struct.pack('!HH', typ, len(value)) + value


def packet(typ, xid, body):
    return struct.pack('!BBH', 0x20, typ, len(body)+4) + xid.to_bytes(3, 'big') + b'\0' + body


def pool(dnn, family=1, identity=b'test-a', slices=False):
    value = ie(177, struct.pack('!H', len(identity)) + identity)
    if dnn is not None:
        value += ie(22, b''.join(bytes([len(label)]) + label.encode() for label in dnn.split('.')))
    if family is not None: value += ie(258, bytes([family]))
    if slices: value += ie(257, b'\1\x11\x22\x33')
    return ie(233, value)


class MockUPF:
    """Independent literal-wire fixture and heartbeat responder, not an encoder round-trip."""
    def __init__(self, address, groups):
        self.address, self.groups = address, groups
        self.node = ie(60, b'\0' + socket.inet_aton(address))
        self.recovery = ie(96, b'\0\0\0\1')
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((address, 8805)); self.sock.settimeout(.1)
        self.responses = queue.Queue(); self.xid = 100
        self.running = True; self.errors = []
        self.thread = threading.Thread(target=self.run)
        self.thread.start(); mocks.append(self)

    def run(self):
        try:
            while self.running:
                try: wire, peer = self.sock.recvfrom(65535)
                except socket.timeout: continue
                xid = int.from_bytes(wire[4:7], 'big')
                if wire[1] == 1:
                    self.sock.sendto(packet(2, xid, self.recovery), peer)
                elif wire[1] == 5:
                    self.sock.sendto(packet(6, xid, self.node + ie(19, b'\1') +
                                           self.recovery + self.groups), peer)
                elif wire[1] == 6:
                    self.responses.put(wire)
        except Exception as error:
            self.errors.append(repr(error))

    def setup(self, groups, accepted=True, repeat=False):
        if not repeat: self.xid += 1
        self.groups = groups
        body = self.node + (self.recovery if accepted else b'') + groups
        self.sock.sendto(packet(5, self.xid, body), ('127.0.0.4', 8805))
        wire = self.responses.get(timeout=5)
        assert int.from_bytes(wire[4:7], 'big') == self.xid
        fields, pos = {}, 8
        while pos < len(wire):
            typ, length = struct.unpack('!HH', wire[pos:pos+4])
            fields[typ] = wire[pos+4:pos+4+length]; pos += 4+length
        assert fields[19] == bytes([1 if accepted else 66]), fields
        assert not self.errors, self.errors

    def stop(self):
        if self.running:
            self.running = False; self.thread.join(timeout=2); self.sock.close()
        assert not self.errors, self.errors


def selected(nf, dnn, address, reason='dnn', source='learned', slot=2, typ=1):
    return nf.command('select %d %s %d' % (slot, dnn, typ), address, reason, source)


try:
    with (OUT / 'selection.pcapng').open('wb') as capture_output:
        capture = subprocess.Popen(['tshark', '-i', 'lo', '-f', 'udp port 8805',
                    '-w', '-'], stdout=capture_output,
                    stderr=(OUT / 'tshark.log').open('w'))
    time.sleep(.3)
    # Real UPFs, no client.upf entry at all. A precedes B so the first result
    # cannot be explained by fallback. No peer has any location match/list.
    smf = NF('dynamic-smf', smf_config(), True)
    a = NF('dynamic-a', upf_config('127.0.0.7', 'internet', 'poola'))
    wait_for(lambda: 'peer[127.0.0.7] generation[1]' in smf.text())
    b = NF('dynamic-b', upf_config('127.0.0.8', 'edge1', 'poolb'))
    wait_for(lambda: 'peer[127.0.0.8] generation[1]' in smf.text())
    first = selected(smf, 'edge1', '127.0.0.8', slot=1)
    assert first['static_count'] == 0 and first['ipv4'].startswith('10.48.0.')
    other = selected(smf, 'internet', '127.0.0.7')
    assert other['ipv4'].startswith('10.45.0.') and other['ipv4'] != first['ipv4']
    results['up-initiated-first-new-allocated-session-no-static-no-location'] = 'PASS'
    c = NF('dynamic-c', upf_config('127.0.0.9', 'edge1', 'poolc'))
    wait_for(lambda: 'peer[127.0.0.9] generation[1]' in smf.text())
    for address in ['127.0.0.8', '127.0.0.9', '127.0.0.8']:
        fresh = selected(smf, 'edge1', address)
        assert fresh['ipv4'] != first['ipv4']  # Same SMF allocator, live uniqueness.
    retained = smf.command('inspect 1', '127.0.0.8')
    assert retained['ipv4'] == first['ipv4'] and retained['pid'] == first['pid']
    results['same-dnn-live-addition-rotation-and-existing-owner'] = 'PASS'
    # C loses PFCP while B remains live. Drop the temporary test session before
    # loss; this test does not exercise baseline release of established UEs.
    smf.command('drop 2')
    c.stop()
    wait_for(lambda: 'PFCP de-associated [127.0.0.9]' in smf.text(), 20)
    selected(smf, 'edge1', '127.0.0.8')
    # An explicit subsequent Setup is required in passive SMF recovery. Use
    # an independent returning-peer fixture at C's same Node ID/endpoint;
    # simply resuming a suspended UPF is not a Setup negotiation.
    returned_c = MockUPF('127.0.0.9', pool('edge1', identity=b'edge1-poolc'))
    returned_c.setup(returned_c.groups)
    wait_for(lambda: 'peer[127.0.0.9] generation[2]' in smf.text(), 20)
    selected(smf, 'edge1', '127.0.0.9')
    retained = smf.command('inspect 1', '127.0.0.8')
    assert retained['ipv4'] == first['ipv4'] and retained['pid'] == first['pid']
    results['actual-heartbeat-loss-explicit-setup-return-with-live-alternative'] = 'PASS'
    smf.command('select 2 missing 1', reason='rr-fallback', source='learned')
    results['no-dnn-match-rr-fallback-recorded-separately'] = 'PASS'
    smf.stop(); a.stop(); b.stop(); returned_c.stop()

    # Both CP-initiated responses and UP-initiated replacements, independent
    # fixtures, and static precedence. Selection injection still uses real SMF
    # session objects and the real allocator, with no competing location.
    for learn in (False, True):
        ma = MockUPF('127.0.0.7', pool('other'))
        mb = MockUPF('127.0.0.8', pool('edge1'))
        clients = [{'address': '127.0.0.7', 'dnn': 'edge1'},
                   {'address': '127.0.0.8', 'dnn': 'other'}]
        smf = NF('precedence-' + str(learn).lower(), smf_config(learn, clients), True)
        wait_for(lambda: smf.text().count('PFCP associated ') >= 2)
        if learn:
            assert smf.text().count('source[6] status[1]') >= 2
        old = selected(smf, 'edge1', '127.0.0.8' if learn else '127.0.0.7',
                       source='learned' if learn else 'static', slot=1)
        results['feature-on-learned-conflict' if learn else 'feature-off-static'] = 'PASS'
        if learn:
            mc = MockUPF('127.0.0.9', pool('edge1'))
            mc.setup(mc.groups)
            ma.setup(pool('edge1')); mb.setup(pool('other'))
            selected(smf, 'edge1', '127.0.0.9')  # cursor B -> C
            selected(smf, 'edge1', '127.0.0.7')  # cursor C -> A
            retained = smf.command('inspect 1', '127.0.0.8')
            assert retained['ipv4'] == old['ipv4'] and retained['pid'] == old['pid']
            results['reassociation-replacement-retains-allocated-owner-address'] = 'PASS'
            ma.setup(b'')
            selected(smf, 'edge1', '127.0.0.9')
            selected(smf, 'edge1', '127.0.0.7', source='static')
            results['accepted-absence-restores-static'] = 'PASS'
            ma.setup(ie(233, b''))
            selected(smf, 'edge1', '127.0.0.9')
            selected(smf, 'edge1', '127.0.0.9')  # would choose A if static revived
            results['invalid-announcement-does-not-revive-static'] = 'PASS'
            ma.setup(pool('edge1') * 17)
            selected(smf, 'edge1', '127.0.0.9')
            results['unsupported-capacity-does-not-revive-static'] = 'PASS'
            ma.setup(ie(233, b'') + pool('edge1'))
            chosen = selected(smf, 'edge1', '127.0.0.7')
            generation = chosen['generation']
            ma.setup(ie(233, b'') + pool('edge1'), repeat=True)
            selected(smf, 'edge1', '127.0.0.9')
            chosen = selected(smf, 'edge1', '127.0.0.7')
            assert chosen['generation'] == generation
            ma.setup(pool('other'), accepted=False)
            selected(smf, 'edge1', '127.0.0.9')
            chosen = selected(smf, 'edge1', '127.0.0.7')
            assert chosen['generation'] == generation
            results['degraded-usable-retransmission-and-rejected-reassociation'] = 'PASS'
            ma.setup(pool('edge1', family=2))
            selected(smf, 'edge1', '127.0.0.9', typ=1)
            selected(smf, 'edge1', '127.0.0.7', typ=2)
            ma.setup(pool('edge1', family=1, identity=b'v4') + pool('edge1', family=2, identity=b'v6'))
            selected(smf, 'edge1', '127.0.0.9')
            dual = selected(smf, 'edge1', '127.0.0.7', typ=3)
            assert dual['ipv4'] and dual['ipv6']
            ma.setup(pool('edge1', family=None, slices=True))
            selected(smf, 'edge1', '127.0.0.9')
            selected(smf, 'edge1', '127.0.0.9')
            results['family-union-dual-stack-and-slice-conservatism'] = 'PASS'
            mc.stop()
        smf.stop(); ma.stop(); mb.stop()

    # Location is deliberately enabled only here. A can win despite a DNN
    # mismatch; changing TAC to a nonmatch attributes the next result to B's DNN.
    ma = MockUPF('127.0.0.7', pool('other'))
    mb = MockUPF('127.0.0.8', pool('edge1'))
    cfg = smf_config(clients=[{'address': '127.0.0.7', 'tac': [101]},
                              {'address': '127.0.0.8'}])
    smf = NF('location-or', cfg, True)
    wait_for(lambda: smf.text().count('PFCP associated ') >= 2)
    smf.command('select 2 edge1 1 101', '127.0.0.7', 'tac', 'learned')
    selected(smf, 'edge1', '127.0.0.8')
    smf.command('select 2 missing 1 101', '127.0.0.7', 'tac', 'learned')
    results['location-or-dnn-with-nonmatching-dnn-and-controlled-negative'] = 'PASS'
    smf.stop(); ma.stop(); mb.stop()

    # Preserve the final first-list-element fallback, even when that first peer
    # is unavailable and a live nonmatching alternative exists.
    mb = MockUPF('127.0.0.8', pool('other'))
    cfg = smf_config(clients=[{'address': '127.0.0.7'}, {'address': '127.0.0.8'}])
    cfg['global'] = copy.deepcopy(GLOBAL)
    cfg['global']['parameter'] = {'no_pfcp_rr_select': True}
    smf = NF('first-peer-fallback', cfg, True)
    wait_for(lambda: 'PFCP associated [127.0.0.8]' in smf.text())
    chosen = selected(smf, 'edge1', '127.0.0.7', reason='first-peer-fallback',
                      source='learned-unavailable')
    assert not chosen['associated']
    results['no-rr-first-unavailable-peer-fallback-preserved'] = 'PASS'
    smf.stop(); mb.stop()
finally:
    for nf in reversed(processes):
        if nf.proc.poll() is None:
            nf.proc.send_signal(signal.SIGCONT); nf.proc.terminate()
            try: nf.proc.wait(timeout=10)
            except subprocess.TimeoutExpired: nf.proc.kill(); nf.proc.wait()
    for mock in mocks: mock.stop()
    if 'capture' in globals():
        capture.send_signal(signal.SIGINT); capture.wait(timeout=10)
    (OUT / 'observations.json').write_text(json.dumps(observations, indent=2))
    (OUT / 'results.json').write_text(json.dumps(results, indent=2))
print(json.dumps(results, indent=2))
