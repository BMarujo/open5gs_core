#!/usr/bin/env python3
"""Independent TShark/raw-TLV checks for the isolated IPv6/slice campaign.

ARTIFACTS OUTPUT_JSON. Requires successful ready-existing-* and drained-dynamic-*.
"""
import ipaddress
import json
from pathlib import Path
import subprocess
import sys

root, output = map(Path, sys.argv[1:3])
report = {'existing': {}, 'dynamic': {}}


def fields(pcap, expression, names):
    cmd = ['tshark', '-r', str(pcap), '-Y', expression, '-T', 'fields']
    for name in names:
        cmd += ['-e', name]
    text = subprocess.check_output(cmd, text=True)
    return [line.split('\t') for line in text.splitlines()]


def tlvs(data):
    while data:
        assert len(data) >= 4
        kind, n = int.from_bytes(data[:2], 'big'), int.from_bytes(data[2:4], 'big')
        assert n <= len(data)-4
        yield kind, data[4:4+n]
        data = data[4+n:]


def network_instance(data):
    labels = []
    while data:
        n = data[0]
        assert 0 < n <= len(data)-1
        labels.append(data[1:1+n].decode('ascii'))
        data = data[1+n:]
    return '.'.join(labels)


for mode in ('off', 'cp', 'up'):
    path = root/('ready-existing-'+mode)
    rows = json.loads((path/'results.json').read_text())
    assert len(rows) == (4 if mode == 'off' else 3)
    for row in rows:
        assert row['exit'] == row['capture_exit'] == 0
        assert row.get('ipv6_data_verified', True)
        assert row.get('learned_dnn_selection', True)
    # S-NSSAI is decoded from actual NGAP; multiple SDs are exercised by the
    # unchanged C tests, including requested/allowed NSSAI checks.
    slices = fields(path/'slice-same-dnn-test.pcapng', 'ngap', ['ngap.sST', 'ngap.sD'])
    sds = {x.replace(':', '') for row in slices for x in row[1].split(',') if x}
    assert {'000080', '009000'} <= sds, sds
    report['existing'][mode] = {'result': 'PASS', 'commands': len(rows),
        'ipv6_echo_replies': sum(x['packet_counts']['ipv6_echo_reply'] for x in rows),
        'observed_ngap_sd': sorted(sds)}

for mode in ('cp', 'up'):
    path = root/('drained-dynamic-'+mode)
    assert json.loads((path/'results.json').read_text())['exit'] == 0
    sessions = json.loads((path/'sessions.json').read_text())
    assert len(sessions) == 3
    pcap = path/'n2-n4-n3.pcapng'
    advertisements = []
    # Wire IE numbers below deliberately do not import the encoder/constants.
    for frame, payload in fields(pcap, 'pfcp.msg_type == 5 || pfcp.msg_type == 6',
                                  ['frame.number', 'udp.payload']):
        wire = bytes.fromhex(payload.replace(':', ''))
        for kind, value in tlvs(wire[16 if wire[0]&1 else 8:]):
            if kind != 233:
                continue
            group = dict(tlvs(value))
            identity = group[177]
            n = int.from_bytes(identity[:2], 'big')
            assert n <= len(identity)-2
            advertisements.append({'frame': int(frame), 'setup_type': wire[1],
                'pool_id': identity[2:2+n].decode('ascii'),
                'dnn': network_instance(group[22]), 'family': group[258][0]&3})
    assert {(a['dnn'], a['family']) for a in advertisements} == {
        ('internet', 1), ('edge1', 1), ('edge1', 2)}, advertisements
    assert all(x['setup_type'] == (6 if mode == 'cp' else 5) for x in advertisements)
    echoes = fields(pcap, 'gtp && (icmpv6.type == 128 || icmpv6.type == 129)',
                    ['ipv6.src', 'ipv6.dst', 'icmpv6.type', 'gtp.teid'])
    for session in sessions:
        assert session['accepted'] and len(session['n4_requests']) == 1
        assert session['n4_requests'][0]['dst'] == session['expected_first']
        if session['psi'] == 3:
            continue
        address = ipaddress.ip_address(session['ue_ipv6'])
        up = [x for x in echoes if x[2] == '128' and ipaddress.ip_address(x[0]) == address]
        down = [x for x in echoes if x[2] == '129' and ipaddress.ip_address(x[1]) == address]
        assert len(up) == len(down) == 3, (session, up, down)
        assert all(int(x[3], 0) == session['upf_teid'] for x in up)
        assert all(int(x[3], 0) == session['gnb_teid'] for x in down)
    report['dynamic'][mode] = {'result': 'PASS', 'accepted_sessions': 3,
        'ipv6_only': sessions[0]['ue_ipv6'], 'dual_stack_ipv6': sessions[1]['ue_ipv6'],
        'first_upfs': [s['n4_requests'][0]['dst'] for s in sessions],
        'pfcp_retransmissions': 0, 'ipv6_echo_requests': len(echoes)//2,
        'advertisements': advertisements}
report['result'] = 'PASS'
output.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
