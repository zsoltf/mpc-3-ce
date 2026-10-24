#!/usr/bin/env python3
"""Record and check the source inputs of a locally composed candidate."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOTS = ('controllers', 'ui', 'vst2', 'firmware', 'scripts')


def blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def record(tree, output):
    files = {}
    for root in ROOTS:
        for path in sorted((tree / root).rglob('*')):
            relative = path.relative_to(tree)
            if not path.is_file() or any(p in ('package', '__pycache__') for p in relative.parts):
                continue
            if path.is_symlink():
                raise SystemExit('Source input is a symlink: ' + str(relative))
            files[str(relative)] = blob(path.read_bytes())
    output.write_text(json.dumps({'format': 1, 'files': files}, indent=2) + '\n')


def verify(repo, manifest, commit):
    if len(commit) != 40 or any(c not in '0123456789abcdef' for c in commit):
        raise SystemExit('Candidate source requires the full lowercase commit SHA')
    resolved = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', commit + '^{commit}'], text=True).strip()
    if resolved != commit:
        raise SystemExit('Candidate source commit differs')
    entries = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-rz', commit, '--', *ROOTS])
    committed = {}
    for entry in entries.split(b'\0'):
        if entry:
            metadata, name = entry.split(b'\t', 1)
            mode, kind, digest = metadata.decode().split()
            if kind == 'blob':
                committed[name.decode()] = digest
    spec = json.loads(manifest.read_text())
    if spec.get('format') != 1 or not spec.get('files'):
        raise SystemExit('Missing composed source input identities')
    for name, digest in spec['files'].items():
        if committed.get(name) != digest:
            raise SystemExit('Composed source differs from candidate commit: ' + name)
        if (repo / name).is_file() and blob((repo / name).read_bytes()) != digest:
            raise SystemExit('Current source differs from composed source: ' + name)
    # The packaging scripts and boot helpers execute outside the composed host.
    for name in ('firmware/package.py', 'firmware/candidate-source.py',
                 'firmware/build-310-candidate.sh', 'firmware/build.sh',
                 'controllers/program-observer/mcu-boot.sh',
                 'controllers/program-observer/mcu-boot-install.sh'):
        if blob((repo / name).read_bytes()) != committed.get(name):
            raise SystemExit('Packaging source differs from candidate commit: ' + name)
    return commit


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == 'record':
        record(Path(sys.argv[2]), Path(sys.argv[3]))
    else:
        raise SystemExit('candidate-source.py record SOURCE_TREE OUTPUT_JSON')
