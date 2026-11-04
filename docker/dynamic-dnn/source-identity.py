#!/usr/bin/env python3
"""Capture commit, tracked diff, untracked files and core input checksums.

REPOSITORY BUILD_SOURCE OUTPUT. Verifies the staged build source's core files.
Documentation and tests are recorded, but excluded from the core code ID so
updating the evidence report does not change the identity of the NF binaries.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo, staged, out = (Path(p).resolve() for p in sys.argv[1:4])
out.mkdir(parents=True, exist_ok=True)


def git(*args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


names = sorted(set(git('ls-files', '-z', '--cached', '--others', '--exclude-standard')
                   .decode().strip('\0').split('\0')))
all_files, core_files = {}, {}
for name in names:
    source = repo / name
    if not source.is_file():
        continue
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    all_files[name] = digest
    if name.startswith(('src/', 'lib/', 'configs/', 'include/')) or name in (
            'meson.build', 'meson_options.txt') or name.endswith('.wrap'):
        assert (staged / name).is_file(), name
        assert hashlib.sha256((staged / name).read_bytes()).hexdigest() == digest, name
        core_files[name] = digest
encoded = json.dumps(core_files, sort_keys=True, separators=(',', ':')).encode()
identity = {'revision': git('rev-parse', 'HEAD').decode().strip(),
            'branch': git('branch', '--show-current').decode().strip(),
            'version': '2.8.0', 'dirty': bool(git('status', '--porcelain')),
            'code_sha256': hashlib.sha256(encoded).hexdigest(),
            'core_inputs': core_files, 'working_files': all_files}
(out / 'source-identity.json').write_text(json.dumps(identity, indent=2, sort_keys=True))
(out / 'tracked.patch').write_bytes(git('diff', '--binary', 'HEAD'))
(out / 'status.txt').write_bytes(git('status', '--short'))
# Preserve every untracked input, including stage 2/3/4A implementation files.
for name in git('ls-files', '-z', '--others', '--exclude-standard').decode().strip('\0').split('\0'):
    if name and (repo / name).is_file():
        dest = out / 'untracked' / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes((repo / name).read_bytes())
print(identity['code_sha256'])
