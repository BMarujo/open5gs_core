#!/usr/bin/env python3
"""Package a tested linux/amd64 build's ELF closure, without apt or rebuild.

Run inside the immutable compiler container: BUILD OUTPUT_CONTEXT.
The caller supplies provenance and verifies source inputs against its build.
"""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

build, out = (Path(p).resolve() for p in sys.argv[1:3])
root = out / 'rootfs'
root.mkdir(parents=True, exist_ok=False)
manifest, closure = {}, {}


def copy(source, dest):
    source = Path(source)
    target = root / dest.lstrip('/')
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if target.exists():
        assert hashlib.sha256(target.read_bytes()).hexdigest() == digest, dest
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target, follow_symlinks=True)
    manifest['/' + str(target.relative_to(root))] = {
        'sha256': digest, 'source': str(source), 'bytes': target.stat().st_size}


for role in ['smf', 'upf']:
    binary = build / f'src/{role}/open5gs-{role}d'
    copy(binary, f'/opt/open5gs/bin/open5gs-{role}d')
    output = subprocess.check_output(['ldd', str(binary)], text=True)
    assert 'not found' not in output, output
    closure[role] = output
    for line in output.splitlines():
        if 'linux-vdso' in line:
            continue
        if '=>' in line:
            name, path = line.strip().split(' => ', 1)
            source = Path(path.split()[0])
            dest = '/opt/open5gs/lib/' + name if source.is_relative_to(build) else str(source)
        else:
            match = re.search(r'(/\S+)', line)
            assert match, line
            source = Path(match[1]); dest = str(source)
        copy(source, dest)

for path in ['/etc/ssl/certs/ca-certificates.crt',
             '/lib/x86_64-linux-gnu/libnss_files.so.2',
             '/lib/x86_64-linux-gnu/libnss_dns.so.2']:
    if Path(path).exists():
        copy(path, path)
for directory in ['tmp', 'etc/open5gs', 'var/log/open5gs']:
    (root / directory).mkdir(parents=True, exist_ok=True)
(root / 'tmp').chmod(0o1777)
(root / 'etc/passwd').write_text('root:x:0:0:root:/root:/sbin/nologin\n')
(root / 'etc/group').write_text('root:x:0:\n')
(root / 'etc/nsswitch.conf').write_text('hosts: files dns\n')
(out / 'payload.json').write_text(json.dumps(manifest, indent=2, sort_keys=True))
(out / 'ldd-build.json').write_text(json.dumps(closure, indent=2))
subprocess.run(['dpkg-query', '-W', '-f=${Package}\t${Version}\t${Architecture}\n'],
               stdout=(out / 'build-packages.tsv').open('w'), check=True)
