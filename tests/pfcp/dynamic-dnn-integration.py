#!/usr/bin/env python3
"""Real NAS/NGAP/SBI/N4 integration, in a disposable network namespace.

BUILD OUTPUT [dynamic|static|incompatible|recovery|recovery-cp].
MongoDB must be local to that namespace.
Requires the dnn-session-driver, actual NF binaries, PyYAML, tshark, SCTP,
NET_ADMIN and /dev/net/tun. No external subscriber or cluster is contacted.
"""
import copy
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

BUILD, OUT = map(lambda p: Path(p).resolve(), sys.argv[1:3])
MODE = sys.argv[3] if len(sys.argv) > 3 else 'dynamic'
assert MODE in ('dynamic', 'static', 'incompatible', 'recovery', 'recovery-cp',
                'asymmetric-smf', 'asymmetric-upf', 'asymmetric-both')
NETWORK_FAILURE = MODE.startswith('asymmetric-')
RECOVERY = MODE in ('recovery', 'recovery-cp') or NETWORK_FAILURE
CP_INITIATOR = MODE in ('recovery-cp', 'asymmetric-upf')
OUT.mkdir(parents=True, exist_ok=NETWORK_FAILURE)  # Network wrapper owns fresh output.
base = yaml.safe_load((BUILD / 'configs/slice.yaml').read_text())
base['global'] = {'max': {'ue': 16, 'peer': 16}}
base['test'] = {'serving': [{'plmn_id': {'mcc': 999, 'mnc': 70}}]}
base['db_uri'] = 'mongodb://127.0.0.1/open5gs'
base['amf']['plmn_support'][0]['s_nssai'] = [{'sst': 1}]
base['nssf']['sbi']['client']['nsi'] = [base['nssf']['sbi']['client']['nsi'][0]]
base['smf'].pop('freeDiameter', None)  # Pure 5GC, no PCRF/EPC daemon needed.
base['smf']['info'] = [{'s_nssai': [{'sst': 1, 'dnn': ['internet', 'edge1', 'nomatch']}]}]
base['smf']['pfcp'] = {'learn_upf_dnn': True, 'server': [{'address': '127.0.0.4'}]}
base['smf']['session'] = [
    {'subnet': subnet, 'gateway': gateway, 'dnn': dnn}
    for dnn, subnet, gateway in [('internet', '10.45.0.0/24', '10.45.0.1'),
                                ('edge1', '10.48.0.0/24', '10.48.0.1'),
                                ('nomatch', '10.50.0.0/24', '10.50.0.1')]]
if RECOVERY:
    base['smf']['time'] = {'message': {'duration': 1000}}
if CP_INITIATOR:
    # Addresses only: CP initiates, DNNs still have to come from PFCP.
    base['smf']['pfcp']['client'] = {'upf': [
        {'address': ip} for ip in ['127.0.0.7', '127.0.0.8', '127.0.0.9']]}
if MODE == 'static':
    # CP initiates Setup. Deliberately conflicting static DNN on A verifies
    # learned precedence; no location lists can explain the selection.
    base['smf']['pfcp']['client'] = {'upf': [
        {'address': '127.0.0.7', 'dnn': 'edge1'},
        {'address': '127.0.0.8', 'dnn': 'internet'}]}
processes, attempts, checks = [], [], {}
capture = None


def wait_for(predicate, timeout=15):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError('condition timeout')


class NF:
    def __init__(self, role, name=None, config=None):
        self.role, self.name = role, name or role
        self.log = OUT / (self.name + '.log')
        cfg = copy.deepcopy(config or base)
        cfg['logger'] = {'level': 'debug', 'file': {'path': str(self.log)}}
        path = OUT / (self.name + '.yaml')
        path.write_text(yaml.safe_dump(cfg, sort_keys=False))
        self.console = (OUT / (self.name + '.console')).open('wb')
        self.proc = subprocess.Popen([str(BUILD / f'src/{role}/open5gs-{role}d'),
            '-c', str(path)], stdout=self.console, stderr=subprocess.STDOUT)
        processes.append(self.proc)
        wait_for(lambda: self.proc.poll() is not None or 'initialize...done' in self.text())
        assert self.proc.poll() is None, self.name

    def text(self):
        return self.log.read_text(errors='replace') if self.log.exists() else ''


def upf(letter, dnn, address, name=None):
    session = copy.deepcopy(next(s for s in base['smf']['session'] if s['dnn'] == dnn))
    session.update(pool_id=dnn + '-' + letter, dev='pool' + letter)
    if MODE == 'incompatible' and letter == 'c':
        session.update(subnet='10.49.0.0/24', gateway='10.49.0.1')
    cfg = {'global': base['global'], 'upf': {
        'pfcp': {'advertise_ue_ip_pools': True, 'server': [{'address': address}]},
        'gtpu': {'server': [{'address': address}]}, 'session': [session]}}
    if RECOVERY:
        cfg['upf']['time'] = {'message': {'duration': 1000}}
    if not CP_INITIATOR and (MODE != 'static' or letter == 'c'):
        cfg['upf']['pfcp']['client'] = {'smf': [{'address': '127.0.0.4'}]}
    return NF('upf', name or 'upf-' + letter, cfg)


def host_route(ip, dev, install_route=True):
    """Lab return path for a shared pool: one /32 to its owning TUN.

    IE 233 does not advertise CIDRs or configure N6. No NAT/route mutation
    occurs outside the disposable namespace. Use rtnetlink, no iproute2 dep.
    """
    def attr(kind, value):
        n = 4 + len(value)
        return struct.pack('HH', n, kind) + value + b'\0' * ((-n) % 4)
    index = socket.if_nametoindex(dev)
    link = struct.pack('BBHiII', socket.AF_UNSPEC, 0, 0, index, 1, 1)
    gateway = '10.45.0.1' if dev == 'poola' else '10.48.0.1'
    if MODE == 'incompatible' and dev == 'poolc':
        gateway = '10.49.0.1'
    address = struct.pack('BBBBI', socket.AF_INET, 24, 0, 0, index)
    address += attr(1, socket.inet_aton(gateway)) + attr(2, socket.inet_aton(gateway))
    body = struct.pack('BBBBBBBBI', socket.AF_INET, 32, 0, 0, 254, 3, 253, 1, 0)
    body += attr(1, socket.inet_aton(ip)) + attr(4, struct.pack('I', index))
    with socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0) as sock:
        sock.settimeout(3)
        updates = [(16, 1 | 4, link), (20, 1 | 4 | 0x400 | 0x200, address)]
        if install_route:
            updates.append((24, 1 | 4 | 0x400 | 0x100, body))
        for typ, flags, payload in updates:
            wire = struct.pack('IHHII', 16 + len(payload), typ, flags, 1, 0) + payload
            sock.sendto(wire, (0, 0))
            reply = sock.recv(4096)
            error = struct.unpack_from('i', reply, 16)[0]
            assert error == 0 or (typ == 20 and error == -17), reply.hex()
    with (OUT / 'return-routes.jsonl').open('a') as f:
        f.write(json.dumps({'ip': ip, 'dev': dev, 'host_route': install_route, 'time': time.time()}) + '\n')


class UE:
    def __init__(self):
        self.console = (OUT / 'ue.console').open('wb')
        cfg = OUT / 'ue.yaml'
        conf = copy.deepcopy(base)
        conf['logger'] = {'level': 'debug', 'file': {'path': str(OUT / 'ue.log')}}
        cfg.write_text(yaml.safe_dump(conf, sort_keys=False))
        self.proc = subprocess.Popen([str(BUILD / 'tests/pfcp/dnn-session-driver'), '-c', str(cfg)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.console, bufsize=0)
        processes.append(self.proc)
        self.pending = b''
        ready = self.result()
        assert ready.get('ready'), ready
        self.imsi = ready['imsi']

    def result(self, timeout=20):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if b'\n' not in self.pending:
                if not select.select([self.proc.stdout], [], [], .1)[0]:
                    assert self.proc.poll() is None, 'UE exited'
                    continue
                raw = os.read(self.proc.stdout.fileno(), 4096)
                assert raw, 'UE EOF'
                self.pending += raw
            raw, self.pending = self.pending.split(b'\n', 1)
            self.console.write(raw + b'\n'); self.console.flush()
            if raw.startswith(b'RESULT '):
                return json.loads(raw[7:])
        raise AssertionError('UE response timeout')

    def command(self, line):
        self.proc.stdin.write((line + '\n').encode())
        return self.result()

    def establish(self, psi, dnn, expected=None, accepted=True):
        record = {'attempt': len(attempts) + 1, 'imsi': self.imsi, 'psi': psi,
                  'dnn': dnn, 'start': time.time(), 'expected_first': expected,
                  'expected_accepted': accepted if expected else None}
        attempts.append(record)  # Persist even an unsuccessful attempt.
        offset = len(smf.text())
        record.update(self.command(f'establish {psi} {dnn}'))
        time.sleep(.2)
        record['end'] = time.time()
        record['selection_logs'] = [s for s in smf.text()[offset:].splitlines() if 'UPF selected DNN[' in s]
        assert len(record['selection_logs']) == 1, record
        if expected:
            assert record['accepted'] == accepted, record
            if accepted:
                assert record['n3'] == expected, record
            assert 'reason[dnn]' in record['selection_logs'][0], record
            assert 'source[learned]' in record['selection_logs'][0], record
        else:
            assert 'fallback]' in record['selection_logs'][0], record
        return record

    def ping(self, record):
        dev = {'127.0.0.7': 'poola', '127.0.0.8': 'poolb', '127.0.0.9': 'poolc'}[record['n3']]
        host_route(record['ue_ip'], dev)
        gateway = '10.45.0.1' if record['dnn'] == 'internet' else '10.48.0.1'
        ping = {'start': time.time()}
        ping.update(self.command(f'ping {record["psi"]} {gateway}'))
        ping['end'] = time.time()
        record.setdefault('pings', []).append(ping)


def decode_evidence():
    # UDP payload is decoded independently of the encoder. tshark also saves
    # a full dissection for manual review (its R19 IE support is limited).
    pcap = OUT / 'n4-n3.pcapng'
    rows = subprocess.check_output(['tshark', '-r', str(pcap), '-Y', 'udp.port == 8805',
        '-T', 'fields', '-e', 'frame.number', '-e', 'frame.time_epoch',
        '-e', 'ip.src', '-e', 'ip.dst', '-e', 'udp.payload'], text=True)
    (OUT / 'n4.tsv').write_text(rows)
    packets = []
    for row in rows.splitlines():
        frame, epoch, src, dst, payload = row.split('\t')
        wire = bytes.fromhex(payload.replace(':', ''))
        header = 16 if wire[0] & 1 else 8
        typ = wire[1]
        packet = {'frame': int(frame), 'time': float(epoch), 'src': src, 'dst': dst,
                  'type': typ, 'sqn': int.from_bytes(wire[header-4:header-1], 'big'),
                  'header_seid': int.from_bytes(wire[4:12], 'big') if header == 16 else None}
        pos = header
        while pos + 4 <= len(wire):
            ie, size = struct.unpack_from('!HH', wire, pos)
            value = wire[pos+4:pos+4+size]
            if ie == 57:
                packet['f_seid'] = int.from_bytes(value[1:9], 'big')
            if ie == 19:
                packet['cause'] = value[0]
            pos += 4 + size
        packets.append(packet)
    (OUT / 'n4.json').write_text(json.dumps(packets, indent=2))
    for record in attempts:
        relevant = [p for p in packets if p['type'] == 50 and p['src'] == '127.0.0.4'
                    and record['start'] <= p['time'] <= record.get('end', time.time())]
        record['requests'] = relevant
        assert relevant, record
        first = relevant[0]
        record['first_n4'] = first['dst']
        record['cp_seid'] = first.get('f_seid')
        record['retransmissions'] = len(relevant) - len({(p['dst'], p['sqn']) for p in relevant})
        if record['expected_first']:
            assert first['dst'] == record['expected_first'], record
            assert all(p['dst'] == first['dst'] for p in relevant), record
        responses = [p for p in packets if p['type'] == 51 and p['sqn'] == first['sqn']
                     and p['src'] == first['dst'] and record['start'] <= p['time']]
        record['responses'] = responses
        assert responses, record
        if record.get('accepted'):
            assert responses[0].get('cause') == 1, record
            record['up_seid'] = responses[0].get('f_seid')
    rows = subprocess.check_output(['tshark', '-r', str(pcap), '-Y', 'gtp && icmp',
        '-T', 'fields', '-e', 'frame.number', '-e', 'frame.time_epoch',
        '-e', 'ip.src', '-e', 'ip.dst', '-e', 'gtp.teid', '-e', 'icmp.type',
        '-e', 'icmp.ident', '-e', 'icmp.seq'], text=True)
    (OUT / 'gtpu-icmp.tsv').write_text(rows)
    echoes = []
    for row in rows.splitlines():
        frame, epoch, src, dst, teid, typ, ident, seq = row.split('\t')
        echoes.append({'frame': int(frame), 'time': float(epoch), 'src': src.split(','),
            'dst': dst.split(','), 'teid': int(teid, 16), 'icmp_type': typ,
            'ident': ident, 'seq': seq})
    for record in attempts:
        for ping in record.get('pings', []):
            requests = [p for p in echoes if ping['start'] <= p['time'] <= ping['end']
                        and p['icmp_type'] == '8' and p['src'][-1] == record['ue_ip']
                        and p['dst'][0] == record['n3'] and p['teid'] == record['upf_teid']]
            replies = [p for p in echoes if ping['start'] <= p['time'] <= ping['end']
                       and p['icmp_type'] == '0' and p['dst'][-1] == record['ue_ip']
                       and p['src'][0] == record['n3'] and p['teid'] == record['gnb_teid']]
            assert len(requests) == len(replies) == 1, (record, requests, replies)
            assert (requests[0]['ident'], requests[0]['seq']) == (replies[0]['ident'], replies[0]['seq'])
            ping['request_frame'], ping['reply_frame'] = requests[0]['frame'], replies[0]['frame']
        if record.get('pings'):
            last_ping = record['pings'][-1]['end']
            assert not any(p['type'] == 54 and p['dst'] == record['n3']
                           and p['header_seid'] == record['up_seid']
                           and record['start'] < p['time'] <= last_ping for p in packets), record
        if record.get('negative_probe'):
            probe = record['negative_probe']
            traffic = [p for p in echoes if probe['start'] <= p['time'] <= probe['end']]
            sent = [p for p in traffic if p['icmp_type'] == '8' and p['src'][-1] == record['ue_ip']
                    and p['dst'][0] == record['n3'] and p['teid'] == record['upf_teid']]
            assert len(sent) == 1 and not any(p['icmp_type'] == '0' for p in traffic), traffic
            probe['request_frame'] = sent[0]['frame']
    checks['gtpu-echo-replies-and-no-early-delete'] = 'PASS'
    with (OUT / 'dissection.txt').open('w') as f:
        subprocess.run(['tshark', '-r', str(pcap), '-V'], stdout=f, check=True)


exit_code = 1
try:
    # Open the output before dumpcap drops privileges. Keep evidence directories
    # private; capture must not require their ownership or mode to be relaxed.
    with (OUT / 'n4-n3.pcapng').open('wb') as capture_output:
        capture = subprocess.Popen(['tshark', '-i', 'lo', '-f',
            'udp port 8805 or udp port 2152', '-w', '-'], stdout=capture_output,
            stderr=(OUT / 'capture.log').open('w'))
    time.sleep(.5)
    for name in ['nrf', 'udr', 'udm', 'ausf', 'pcf', 'nssf', 'bsf', 'amf']:
        NF(name)
    # CP initiated case starts UPFs first and omits their client.smf list.
    if MODE == 'static':
        a = upf('a', 'internet', '127.0.0.7')
        b = upf('b', 'edge1', '127.0.0.8')
    smf = NF('smf')
    smf_pid = smf.proc.pid
    if MODE != 'static':
        a = upf('a', 'internet', '127.0.0.7')
    wait_for(lambda: 'PFCP associated [127.0.0.7]' in smf.text())
    time.sleep(2)  # Let NRF registration finish before NAS registration.
    ue = UE()
    internet = ue.establish(1, 'internet', '127.0.0.7')
    ue.ping(internet)
    if MODE != 'static':
        b = upf('b', 'edge1', '127.0.0.8')
    wait_for(lambda: 'PFCP associated [127.0.0.8]' in smf.text())
    ue.ping(internet)  # Established A session survives adding B.
    edge_b = ue.establish(2, 'edge1', '127.0.0.8')
    ue.ping(edge_b)
    if MODE != 'static':
        c = upf('c', 'edge1', '127.0.0.9')
        wait_for(lambda: 'PFCP associated [127.0.0.9]' in smf.text())
        ue.ping(internet)
        if NETWORK_FAILURE:
            direction = MODE.removeprefix('asymmetric-')
            old_c = None
            if direction == 'upf':
                old_c = ue.establish(3, 'edge1', '127.0.0.9')
                ue.ping(old_c)
            offsets = {'smf': len(smf.text()), 'upf': len(c.text())}
            target = c if direction == 'upf' else smf
            fault_events = []
            def fault(action):
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
                    sock.connect(str(OUT/'control.sock'))
                    sock.sendall((json.dumps({'action': action, 'direction': direction})+'\n').encode())
                    result = json.loads(sock.makefile('r').readline())
                    assert result['ok'], result
                    fault_events.append({'time': time.time(), 'action': action, 'result': result})
                    return result
            fault('drop_with_setup')
            wait_for(lambda: 'PFCP de-associated' in target.text()[offsets['upf' if direction=='upf' else 'smf']:],30)
            ue.ping(internet); ue.ping(edge_b)
            if direction == 'smf':
                # C has no session to release. Controlled cursor and blocked
                # Setup replies demonstrate exclusion while locally unassociated.
                edge = ue.establish(3, 'edge1', '127.0.0.8')
                ue.ping(edge)
            fault('clear')
            wait_for(lambda: c.text().rfind('PFCP associated')>c.text().rfind('PFCP de-associated'),30)
            confirmed = 'PFCP learned pools heartbeat confirmed peer[127.0.0.9] generation['
            wait_for(lambda: confirmed in smf.text()[offsets['smf']:],30)
            if old_c: ue.ping(old_c)
            expected = ['127.0.0.8','127.0.0.9']*2 if old_c else ['127.0.0.9','127.0.0.8']*2
            returned=[]
            for psi, ip in zip([4,5,7,8],expected):
                edge=ue.establish(psi,'edge1',ip)
                ue.ping(edge);ue.ping(internet);ue.ping(edge_b);returned.append(edge)
            time.sleep(11)
            for edge in returned:ue.ping(edge)
            ue.ping(internet);ue.ping(edge_b)
            assert c.proc.poll() is None
            (OUT/'fault-events.json').write_text(json.dumps(fault_events,indent=2))
            checks['real-peer-selective-loss-automatic-reassociation'] = 'PASS'
            checks['new-session-first-destinations-after-network-recovery'] = 'PASS'
        elif RECOVERY:
            # C has no UE session yet; loss is not presented as preservation
            # of sessions on a restarted UPF. A/B sessions remain established.
            c.proc.terminate(); assert c.proc.wait(timeout=10) == 0
            wait_for(lambda: 'PFCP de-associated [127.0.0.9]' in smf.text(), 30)
            edge = ue.establish(3, 'edge1', '127.0.0.8')
            ue.ping(edge); ue.ping(internet)
            offset = len(smf.text())
            c = upf('c', 'edge1', '127.0.0.9', 'upf-c-return')
            # The active CP's existing restart path may negotiate Setup again
            # (generation 3). Wait for actual confirmation, not a fixed count.
            confirmed = 'PFCP learned pools heartbeat confirmed peer[127.0.0.9] generation['
            wait_for(lambda: confirmed in smf.text()[offset:], 30)
            confirmed_offset = smf.text().rfind(confirmed)
            (OUT / 'returned-peer-confirmation.txt').write_text(
                smf.text()[confirmed_offset:].splitlines()[0] + '\n')
            # Capture multiple new NAS sessions, not just one expected turn.
            # Both existing A/B sessions must still pass data throughout.
            returned = []
            for psi, expected in [(4, '127.0.0.9'), (5, '127.0.0.8'),
                                  (7, '127.0.0.9'), (8, '127.0.0.8')]:
                edge = ue.establish(psi, 'edge1', expected)
                ue.ping(edge); ue.ping(internet); ue.ping(edge_b)
                returned.append(edge)
            # Exercise the next periodic Heartbeat as well as the initial
            # confirmation; an immediately re-erased mapping is not a pass.
            time.sleep(11)
            assert 'PFCP learned pools invalidated' not in smf.text()[confirmed_offset:]
            assert smf.proc.poll() is None and c.proc.poll() is None
            ue.ping(returned[0]); ue.ping(returned[2]); ue.ping(internet)
            checks['heartbeat-loss-upf-restart-return-real-nas-sessions'] = 'PASS'
            checks['returned-peer-four-new-sessions-C-B-C-B'] = 'PASS'
            checks['returned-peer-retained-after-next-periodic-heartbeat'] = 'PASS'
        elif MODE == 'incompatible':
            bad = ue.establish(3, 'edge1', '127.0.0.9')
            # Baseline UPF accepts CP addresses by DNN even outside its CIDR.
            # Leave normal /24 return routing in place, instead of installing
            # an artificial /32 that would conceal the incompatible plan.
            host_route(bad['ue_ip'], 'poolc', install_route=False)
            (OUT / 'incompatible-routes.txt').write_text(Path('/proc/net/route').read_text())
            probe = {'start': time.time()}
            probe.update(ue.command('probe 3 10.49.0.1'))
            probe['end'] = time.time()
            bad['negative_probe'] = probe
            edge = ue.establish(4, 'edge1', '127.0.0.8')
            ue.ping(edge)
            checks['same-dnn-incompatible-cidr-not-detectable-by-ie233'] = 'PASS (negative characterization)'
        else:
            for psi, expected in [(3, '127.0.0.9'), (4, '127.0.0.8'), (5, '127.0.0.9')]:
                edge = ue.establish(psi, 'edge1', expected)
                ue.ping(edge)
    unknown = ue.establish(6, 'nomatch')
    checks['legacy-no-dnn-match'] = 'PASS (fallback observed; session outcome recorded separately)'
    assert smf.proc.pid == smf_pid and smf.proc.poll() is None
    assert len({a['ue_ip'] for a in attempts if a.get('accepted')}) == sum(a.get('accepted', False) for a in attempts)
    checks['real-session-selection-and-owned-return-routing'] = 'PASS'
    checks['smf-not-restarted'] = 'PASS'
    ue.proc.stdin.close()
    assert ue.proc.wait(timeout=10) == 0
    exit_code = 0
except Exception:
    (OUT / 'failure.txt').write_text(traceback.format_exc())
    traceback.print_exc()
finally:
    # Close capture before destroying NFs so shutdown is not confused with a
    # session deletion during live addition. Only processes created here.
    if capture:
        capture.send_signal(signal.SIGINT)
        capture.wait(timeout=10)
    try:
        decode_evidence()
        checks['first-pfcp-request-per-attempt'] = 'PASS'
    except Exception:
        checks['first-pfcp-request-per-attempt'] = 'FAIL'
        (OUT / 'evidence-failure.txt').write_text(traceback.format_exc())
        exit_code = 1
    for proc in reversed(processes):
        if proc.poll() is None:
            proc.send_signal(signal.SIGCONT)
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait()
    (OUT / 'sessions.json').write_text(json.dumps(attempts, indent=2))
    (OUT / 'results.json').write_text(json.dumps({'mode': MODE, 'exit': exit_code, 'checks': checks}, indent=2))
    subprocess.run(['tshark', '--version'], stdout=(OUT / 'decoder-version.txt').open('w'), check=True)
sys.exit(exit_code)
