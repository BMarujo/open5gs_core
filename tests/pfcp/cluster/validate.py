#!/usr/bin/env python3
"""Offline checks for rendered fixtures and the rollback ownership guard.

No cluster is contacted. kubectl is replaced by a local fake for guard tests.
Usage: validate.py OUTPUT (fresh directory).
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=False)
ns, lab = 'open5gs-dnn-lab-example', 'stage5-example'
common = ['--namespace', ns, '--lab-id', lab,
          '--smf-image', 'open5gs-smf:2c80f8f2ba9d-dirty-e62810f8908e',
          '--upf-image', 'open5gs-upf:2c80f8f2ba9d-dirty-e62810f8908e',
          '--tools-image', 'open5gs-pfcp-codec-build:20261007',
          '--nrf-uri', 'http://nrf.' + ns + '.svc.cluster.local:7777',
          '--smf-n4', '192.0.2.4']
ids = []
for role in ['namespace', 'smf', 'upf-a', 'upf-b', 'upf-c']:
    dest = out / (role + '.json')
    subprocess.run([sys.executable, str(here / 'render.py'), *common,
                    '--role', role, '--output', str(dest)], check=True)
    objects = json.loads(dest.read_text())['items']
    if role == 'namespace':
        assert objects[0]['metadata']['name'] == ns
        policy = objects[1]['spec']
        assert policy['ingress'] == [{'from': [{'podSelector': {}}]}]
        assert policy['egress'][0] == {'to': [{'podSelector': {}}]}
        continue
    config, pod = objects
    nf = 'smf' if role == 'smf' else 'upf'
    template = config['data']['template.json']
    rendered = json.loads(template.replace('$POD_IP', '192.0.2.100'))
    assert rendered[nf]['pfcp']['server'][0]['address'] == '192.0.2.100'
    # Compile the exact init command without executing paths outside this test.
    compile(pod['spec']['initContainers'][0]['command'][2], '<config-init>', 'exec')
    assert pod['spec']['automountServiceAccountToken'] is False
    assert not pod['spec'].get('hostNetwork')
    assert all(c['imagePullPolicy'] == 'Never' for c in pod['spec']['containers'])
    if nf == 'smf':
        assert 'client' not in rendered[nf]['pfcp']
        assert rendered[nf]['pfcp']['learn_upf_dnn'] is True
    else:
        pool = rendered[nf]['session'][0]
        assert len(pool['pool_id'].encode('ascii')) <= 64
        ids.append(pool['pool_id'])
assert len(set(ids)) == 3

# Check boundary length and forbidden discovery namespace without Kubernetes.
for lab_id, uri, accepted in [
        ('a' * 58, common[common.index('--nrf-uri') + 1], True),
        ('a' * 59, common[common.index('--nrf-uri') + 1], False),
        (lab, 'http://nrf.production.svc.cluster.local:7777', False)]:
    args = common.copy()
    args[args.index('--lab-id') + 1] = lab_id
    args[args.index('--nrf-uri') + 1] = uri
    result = subprocess.run([sys.executable, str(here / 'render.py'), *args,
                             '--role', 'smf', '--output', str(out / 'boundary.json')],
                            capture_output=True, text=True)
    assert (result.returncode == 0) == accepted
(out / 'boundary.json').unlink(missing_ok=True)

with tempfile.TemporaryDirectory() as temp:
    fake = Path(temp) / 'kubectl'
    fake.write_text('''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
args=sys.argv[1:]
if 'get' in args:
    print(os.environ['FAKE_UID'] if 'metadata.uid' in args[-1] else os.environ['FAKE_LAB'])
elif 'delete' in args:
    body=json.loads(sys.stdin.read())
    assert body['preconditions']['uid']=='owned-uid'
    assert args[args.index('--raw')+1]=='/api/v1/namespaces/open5gs-dnn-lab-example'
    Path(os.environ['FAKE_DELETE']).write_text(json.dumps({'args':args,'body':body}))
else: raise SystemExit(99)
''')
    fake.chmod(0o755)
    marker = out / 'simulated-delete.json'
    for uid, label, expected, delete in [
            ('owned-uid', lab, 'owned-uid', True),
            ('reused-name-uid', lab, 'owned-uid', False),
            ('owned-uid', 'other-lab', 'owned-uid', False)]:
        marker.unlink(missing_ok=True)
        env = dict(os.environ, PATH=temp + ':' + os.environ['PATH'],
                   FAKE_UID=uid, FAKE_LAB=label, FAKE_DELETE=str(marker))
        result = subprocess.run(['bash', str(here / 'rollback.sh'), 'fake-context',
                                 ns, lab, expected], env=env, capture_output=True, text=True)
        assert (result.returncode == 0) == delete
        assert marker.exists() == delete
    # Retain a positive simulated delete body, explicitly labeled as simulated.
    env.update(FAKE_UID='owned-uid', FAKE_LAB=lab)
    subprocess.run(['bash', str(here / 'rollback.sh'), 'fake-context', ns, lab,
                    'owned-uid'], env=env, check=True)

(out / 'validation.txt').write_text(
    'PASS offline: JSON fixtures, init command syntax, Pod IP substitution, '
    'isolation fields, no SMF static peers, stable unique IDs, ID length limit, '
    'NRF namespace restriction. PASS simulated rollback: label/UID rejection '
    'and delete UID precondition. Kubernetes API/admission/CNI/device behavior NOT RUN.\n')
print((out / 'validation.txt').read_text(), end='')
