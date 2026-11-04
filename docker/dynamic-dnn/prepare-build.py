#!/usr/bin/env python3
"""REPO VENDOR_SOURCE ARTIFACTS [--baseline]: stage sources outside checkout.

The two Meson git subprojects must already be available at the pinned commits.
Their git archives avoid copying cached objects, build directories or .git.
The normal path includes all existing tracked/untracked work, including the
feature's new C files. --baseline uses the real HEAD without those changes.
"""
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile

repo, vendor, out = (Path(p).resolve() for p in sys.argv[1:4])
baseline = len(sys.argv) > 4 and sys.argv[4] == '--baseline'
source = out / 'source'
source.mkdir(parents=True, exist_ok=False)


def git(path, *args):
    return subprocess.check_output(['git', '-c', 'safe.directory=' + str(path), '-C', str(path), *args])


revision = git(repo, 'rev-parse', 'HEAD').decode().strip()
with tarfile.open(fileobj=io.BytesIO(git(repo, 'archive', revision))) as archive:
    archive.extractall(source, filter='data')
if not baseline:
    names = git(repo, 'ls-files', '-z', '--cached', '--others', '--exclude-standard').decode().split('\0')
    for name in names:
        if not name:
            continue
        current, target = repo / name, source / name
        if current.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(current.read_bytes())
            target.chmod(current.stat().st_mode)
        elif not current.exists() and target.is_file():
            target.unlink()
commits = {'freeDiameter': '14725af3ba0edbf9ff61c4e3239ed42464423b2e',
           'prometheus-client-c': 'a58ba25bf87a9b1b7c6be4e6f4c62047d620f402'}
for name, commit in commits.items():
    local = vendor / 'subprojects' / name
    assert git(local, 'rev-parse', 'HEAD').decode().strip() == commit, name
    target = source / 'subprojects' / name
    target.mkdir(parents=True, exist_ok=False)
    with tarfile.open(fileobj=io.BytesIO(git(local, 'archive', commit))) as archive:
        archive.extractall(target, filter='data')
(out / 'staging.json').write_text(json.dumps({'revision': revision, 'baseline': baseline,
                                            'subprojects': commits}, indent=2))
