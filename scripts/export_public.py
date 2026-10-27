#!/usr/bin/env python3
"""Refresh the local public checkout from the reviewed explicit export list.
Does not initialize Git, rewrite history, or push. Removes only tracked export
files omitted from the list; refuses a dirty checkout before making changes.
"""
import argparse, hashlib, json, pathlib, re, shutil, subprocess
repo = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', type=pathlib.Path, default=repo)
parser.add_argument('--archive-directory', type=pathlib.Path, help='Create a new plain source snapshot without touching the public checkout')
parser.add_argument('--candidate-310', metavar='SOURCE_COMMIT', help='Export a private v0.4.2 source candidate at this exact commit')
args = parser.parse_args()
repo = args.source_root.resolve()
dest = args.archive_directory.resolve() if args.archive_directory else repo / 'artifacts/mpc-3-community-edition'
if args.archive_directory and dest.exists():
    raise SystemExit('Archive directory must not exist')
files = json.loads((repo / 'release/public-files.json').read_text())
candidate_metadata = None
if args.candidate_310:
    commit = args.candidate_310
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise SystemExit('Candidate export requires the full source commit SHA')
    tree = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-rz', commit])
    committed = {}
    for item in tree.split(b'\0'):
        if item:
            info, name = item.split(b'\t', 1)
            committed[name.decode()] = info.decode().split()[2]
    for source in set(files.values()):
        data = (repo/source).read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if committed.get(source) != blob:
            raise SystemExit('Candidate export differs from source commit: ' + source)
    def version(path, name):
        return re.search(r'^#define ' + name + r' ([0-9]+)u$', (repo/path).read_text(), re.M).group(1)
    candidate_metadata = {
        'release': 'v0.4.2', 'image_revision': 'v0_4_2', 'firmware': '3.10.0 Gen1',
        'product_candidate': commit, 'development_source_base': commit,
        'runtime_format': 'CMD' + version('controllers/program-observer/command-state.h', 'COMMAND_VERSION') + '/MMV' + version('controllers/program-observer/mirror-state.h', 'MIRROR_VERSION'),
        'status': 'private-development-candidate', 'image_flash_tested': False,
        'qualified_hardware': 'Unqualified MPC 3.10 development candidate',
        'runtime_location': '/usr/share/mpclearn/mcu',
        'release_artifacts_current_for_source': False,
        'runtime_checksums': None,
    }

def git(*args):
    return subprocess.check_output(['git', '-C', str(dest), *args], text=True)
if not args.archive_directory and git('status', '--porcelain').strip():
    raise SystemExit('Public checkout has local changes; reconcile them first.')
for target, source in files.items():
    for value in (target, source):
        p = pathlib.PurePosixPath(value)
        if p.is_absolute() or '..' in p.parts or '.git' in p.parts:
            raise SystemExit('Invalid export path: ' + value)
    if not (repo/source).is_file() or (repo/source).is_symlink():
        raise SystemExit('Missing/plain-file source required: ' + source)
# Closure: the allow-list silently omits anything not named, and it did. Two
# published releases (v0.1.0-rc.3, v0.2.0) shipped source whose capture file
# included .inc files that were never exported, so the public tree could not be
# built. Refuse to export when an exported file references, by quoted #include
# or by ./script invocation, a file that exists in this repository but is not
# in the list. Generated build outputs are exempt because the build creates them.
import os, re
GENERATED = {'config.h', 'pinned.h', 'module-ranges.h'}
include = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
script = re.compile(r'(?:^|[\s;&|(])\./([A-Za-z0-9_.\-]+)')
exported = set(files)
gaps = {}
for target, source in files.items():
    path = repo/source
    if path.suffix in ('.c', '.cc', '.cpp', '.h', '.hpp', '.inc', '.S'):
        names = include.findall(path.read_text(errors='replace'))
    elif path.suffix == '.sh':
        names = script.findall(path.read_text(errors='replace'))
    else:
        continue
    folder = pathlib.PurePosixPath(target).parent
    for name in names:
        if pathlib.PurePosixPath(name).name in GENERATED:
            continue
        # Normalise so '../stop-route.h' from a subdirectory matches its entry.
        wanted = os.path.normpath(str(folder/name))
        if wanted not in exported and (repo/wanted).is_file():
            gaps.setdefault(wanted, []).append(target)
if gaps:
    for wanted, users in sorted(gaps.items()):
        print('Not exported but required: ' + wanted + ' (used by ' + ', '.join(sorted(users)) + ')')
    raise SystemExit('Public tree would not build; add the files above to release/public-files.json.')
for target in ([] if args.archive_directory else git('ls-files', '-z').split('\0')):
    if target and target not in files:
        (dest/target).unlink()
for target, source in files.items():
    output = dest/target
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo/source, output)
if candidate_metadata:
    for target in ('SOURCE.json', 'release/public-source.json'):
        (dest/target).write_text(json.dumps(candidate_metadata, indent=2) + '\n')
print(f'Exported {len(files)} files to {dest}; review and commit before pushing.')
