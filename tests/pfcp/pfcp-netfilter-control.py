#!/usr/bin/env python3
"""Host-side lab controller: HELPER_CONTAINER SOCKET_PATH.

The helper MUST share a disposable --network none test container's network
namespace. Only a private nftables table in that namespace is touched. The
protocol accepts fixed test operations, never shell commands or arbitrary rules.
"""
import json
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time

helper, path = sys.argv[1], Path(sys.argv[2])
upf_ip = sys.argv[3] if len(sys.argv)>3 else '127.0.0.7'
assert upf_ip in ('127.0.0.7', '127.0.0.9')
table = 'dnn_pfcp_test'

def nft(script=None):
    args = ['docker', 'exec', '-i', helper, 'nft']
    args += ['-f', '-'] if script is not None else ['-j', 'list', 'table', 'ip', table]
    result = subprocess.run(args, input=script, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    return result.stdout

nft(f'add table ip {table}\nadd chain ip {table} output {{ type filter hook output priority 0; policy accept; }}\n')
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(str(path)); server.listen(1); server.settimeout(.5)
running = True

def stop(*args):
    global running
    running = False

signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)
try:
    while running:
        try:
            conn, _ = server.accept()
        except socket.timeout:
            continue
        with conn:
            req = {}
            try:
                req = json.loads(conn.makefile('r').readline())
                before = json.loads(nft())
                action = req['action']
                if action in ('drop', 'drop_once', 'drop_with_setup'):
                    direction = req['direction']
                    pairs = {'smf': [(upf_ip, '127.0.0.4')],
                             'upf': [('127.0.0.4', upf_ip)],
                             'both': [(upf_ip, '127.0.0.4'), ('127.0.0.4', upf_ip)]}
                    assert direction in pairs
                    rules = f'flush chain ip {table} output\n'
                    for src, dst in pairs[direction]:
                        limit = 'limit rate 1/minute burst 1 packets ' if action == 'drop_once' else ''
                        types = '{ 2, 6 }' if action=='drop_with_setup' else '2'
                        rules += (f'add rule ip {table} output ip saddr {src} ip daddr {dst} '
                                  f'udp dport 8805 @th,72,8 {types} {limit}counter drop\n')
                    nft(rules)
                elif action == 'clear':
                    nft(f'flush chain ip {table} output\n')
                else:
                    assert action == 'status'
                response = {'ok': True, 'before': before, 'after': json.loads(nft())}
            except Exception as error:
                response = {'ok': False, 'error': str(error)}
            with path.with_suffix('.jsonl').open('a') as log:
                log.write(json.dumps({'time': time.time(), 'request': req, 'response': response})+'\n')
            conn.sendall((json.dumps(response)+'\n').encode())
finally:
    server.close(); path.unlink(missing_ok=True)
    nft(f'delete table ip {table}\n')
