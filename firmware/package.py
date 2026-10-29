#!/usr/bin/env python3
"""Copy only the accepted package files; never archive a device stage or settings."""
import argparse, hashlib, importlib.util, json, pathlib, re, shutil
repo = pathlib.Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--prerelease', action='store_true')
# A development candidate for another firmware names its own unqualified
# manifest explicitly; the accepted 3.9.1 release manifests stay the gate.
p.add_argument('--candidate-310', type=pathlib.Path, metavar='MANIFEST')
p.add_argument('--source-commit', help='Exact committed source of a 3.10 candidate')
p.add_argument('--source-inputs', type=pathlib.Path, help='Composition source-inputs.json')
p.add_argument('source', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
args = p.parse_args()
if args.candidate_310 and not args.prerelease:
    raise SystemExit('A 3.10 candidate is a prerelease payload')
if args.candidate_310:
    if not args.source_commit or not args.source_inputs:
        raise SystemExit('A 3.10 candidate requires --source-commit and --source-inputs')
    spec = importlib.util.spec_from_file_location('candidate_source', repo/'firmware/candidate-source.py')
    provenance = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(provenance)
    provenance.verify(repo, args.source_inputs, args.source_commit)
    def protocol(path, name):
        match = re.search(r'^#define ' + name + r' ([0-9]+)u$', (repo/path).read_text(), re.M)
        if not match:
            raise SystemExit('Missing source protocol version: ' + name)
        return match.group(1)
    candidate_format = 'CMD' + protocol('controllers/program-observer/command-state.h', 'COMMAND_VERSION') + '/MMV' + protocol('controllers/program-observer/mirror-state.h', 'MIRROR_VERSION')
elif args.source_commit or args.source_inputs:
    raise SystemExit('Source options require --candidate-310')
# This legacy prerelease metadata stays paired with the frozen 3.9 runtime pins.
# The explicit 3.10 candidate overrides its build identity below.
release = json.loads((repo/'release/public-source-v0.3.0.json').read_text()) if args.prerelease else {}
RUNTIME_SOURCE = 'v0.4.3' if args.candidate_310 else release['release'] if args.prerelease else 'v0.2.6'
source, output = args.source, args.output
names = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button'.split()
if args.prerelease:
    names += ['plugin-setup', 'plugin-bundle.json']
manifest = {}
for line in (source/'session-package.sha256').read_text().splitlines():
    digest, name = line.split()
    if name in manifest or name not in names or len(digest) != 64:
        raise SystemExit('Invalid package manifest')
    manifest[name] = digest
if set(manifest) != set(names):
    raise SystemExit('Incomplete package')
for name in names:
    if (source/name).is_symlink() or hashlib.sha256((source/name).read_bytes()).hexdigest() != manifest[name]:
        raise SystemExit('Mismatched package: '+name)
# Freeze every executable/helper, not just the observer. A self-consistent mixed
# package is not the accepted runtime.
qualified = dict((name, digest) for digest, name in
                 (line.split() for line in (args.candidate_310 or repo/'firmware'/('prerelease-runtime.sha256' if args.prerelease else 'runtime.sha256')).read_text().splitlines()))
if manifest != qualified:
    raise SystemExit('Not the accepted '+RUNTIME_SOURCE+' package; qualify a new release first')
LOCATION = '/usr/share/mpclearn/mcu'
config = (source/'config.h').read_text().splitlines()
if '#define WINDOW_SECONDS 0u' not in config:
    raise SystemExit('Timed verification packages cannot be released')
for line in ['#define OBSERVER_LIBRARY "'+LOCATION+'/command-observer.so"', '#define OBSERVER_LOG "/run/mpclearn/state/volume.state"', '#define COMMAND_PATH "/run/mpclearn/state/command.state"']:
    if line not in config:
        raise SystemExit('Not built for the image location: '+line)
if '#define NATIVE_PREFERENCES_STATE "/run/mpclearn/state/native-preferences.state"' not in config:
    raise SystemExit('Native Preferences state path is absent from the image runtime')
observer = (source/'command-observer.so').read_bytes()
if b'native_preferences_install\0' not in observer:
    raise SystemExit('Native Preferences observer is absent from the image runtime')
# The Pd/Cardinal touch editors are compiled into the observer; a plain Native
# Preferences build silently ships plugins without editors (rc.1 and v0.3.0).
if args.prerelease and not all(b'\0'+api+b'\0' in observer for api in (b'mpclearn_pd_hosted_api', b'mpclearn_cardinal_touch_api')):
    raise SystemExit('Pd/Cardinal editors are absent from the observer; build it with vst2/pd-rack-prototype/build.sh')
output.mkdir(parents=True, exist_ok=False)
for name in names + ['session-package.sha256']:
    shutil.copyfile(source/name, output/name)
for name in ['mcu-boot.sh','mcu-boot-install.sh']:
    shutil.copyfile(repo/'controllers/program-observer'/name, output/name)
shutil.copyfile(repo/'controllers/program-observer/LICENSE', output/'LICENSE')
metadata = {'runtime_source':RUNTIME_SOURCE, 'firmware':'3.10.0 Gen1' if args.candidate_310 else '3.9.1 Gen1', 'qualified_hardware':'MPC Live II + full X-Touch MC/USB', 'location':LOCATION, 'format':'CMD31/MMV17', 'fresh_device_setup':'Choose the controller in MPC Preferences > CONTROLLERS. Global MIDI Learn and XMM profiles are not required. Disable raw X-TOUCH_INT musical input; see guide.'}
if args.prerelease:
    metadata.update(image_revision=release['image_revision'], product_candidate=release['product_candidate'],
                    format=release['runtime_format'],
                    qualified_hardware=release['qualified_hardware'],
                    plugin_bundle='MPCLEARN/plugins.squashfs on the SD card',
                    base_label='CE v0.4.3 candidate' if args.candidate_310 else 'CE v0.2.6',
                    limits='Cardinal CPU-heavy patches can overload the MPC. Existing controller hardware and prototype qualification limits apply.')
if args.candidate_310:
    metadata.update(image_revision='v0_4_3', product_candidate=args.source_commit,
                    format=candidate_format, qualified_hardware='Unqualified MPC 3.10 development candidate',
                    source_inputs_sha256=hashlib.sha256(args.source_inputs.read_bytes()).hexdigest())
(output/'BUILD.json').write_text(json.dumps(metadata, indent=2)+'\n')
(output/'payload.sha256').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in sorted(output.iterdir())))
