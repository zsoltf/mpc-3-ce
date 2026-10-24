# MCU image and release

The current release preparation is **v0.4.0 for MPC 3.10**, CMD32/MMV21,
with provision revision `v0_4_0`. See the
[3.10 build instructions](../docs/BUILDING.md#mpc-310-candidate-runtime)
and [release notes](../docs/releases/v0.4.0.md). The runtime runs from
`/usr/share/mpclearn/mcu` and uses the matching
[SD-card Pd/Cardinal bundle](plugin-setup/README.md).

`release/public-source.json` owns subsequent release qualification and names the
exact runtime/payload checksums and three 3.10 browser recipes. The payload's
`BUILD.json` remains its original build-time candidate record; promotion does not
rewrite installed bytes. See [Publishing](../docs/PUBLISHING.md) for finalization.

The retained 3.9.1 path uses `mpc-ce.json`, `prerelease-runtime.sha256`,
`release/public-source-v0.3.0.json` and the original browser recipes. Its historical
Info label is `MPC CE v0.2.6`. The default `package.py` mode, `runtime.sha256` and
`build-v0.2.6.sh` retain the older v0.2.6 identity; use that workflow only from its
matching frozen source.

## Deliverables

- A **personal** firmware image containing the exact accepted MCU binaries,
  which run in place from the image, the boot/session helpers and a boot-time
  provisioner.
- A standalone MCU payload without Akai firmware, credentials, settings,
  projects, plug-ins or session data.
- The source and build instructions. No firmware redistribution permission is
  implied by source availability.

The browser builder disables SSH by default, including a system service mask.
An optional owner-supplied Ed25519 public key enables SSH for that owner to log
in as root. Such a personalized image must not be shared as a generic community image.
Private client keys never enter the image. Each MPC generates its own persistent
SSH host identity. Password authentication and the vendor CA route are disabled.

## Developer build environment

These Docker commands are for maintainers preparing and verifying patch data.
The browser-only end-user builder is developed separately under `builder/`; its users
must not need Docker, Python, a compiler, VM, WSL or terminal commands.

### Reproduce the frozen public reference image

Run the commands in this subsection only from the frozen v0.2.6 release source
with its matching manifest, admission and qualified runtime pins, not current
`main`. First follow the complete
[runtime build instructions](../docs/BUILDING.md). Docker is required. Supply your own official3.9.1 Gen1 USB update, pinned SHA256:
`4c0f7797a006313fad206f9513eca8b3823e834c9e0941a6b5f088386cb835d6`.
Do not substitute another firmware version.

```sh
mkdir -p inputs
# Copy your official download to inputs/MPC-3.9.1-Gen1-update.img
# Copy your own Ed25519 PUBLIC key to inputs/owner.pub
# Obtain the image tool from its original upstream repository first:
git clone https://github.com/TheKikGen/MPC-LiveXplore.git ../MPC-LiveXplore
sh firmware/prepare-upstream.sh ../MPC-LiveXplore
docker build -t mpclearn-build:local .
# Extract the pristine MPC once, then create the exact separate CE executable.
docker run --rm --network none -v "$PWD:/work" -w /work mpclearn-build:local sh -ec '
python3 scripts/image_format.py inputs/MPC-3.9.1-Gen1-update.img build/rootfs.original.ext
debugfs -R "dump /usr/bin/MPC /work/build/MPC" build/rootfs.original.ext
'
python3 firmware/patch-mpc-ce.py build/MPC build/v0_2_6/MPC
./controllers/program-observer/native-preferences-build.sh \
  "$PWD/build/v0_2_6/MPC" "$PWD/build/MPC" /usr/share/mpclearn/mcu
# After the component and native qualification gates, maintainers build the
# local release roots, images and three browser recipes together:
sh firmware/build-v0.2.6.sh
```

### Build v0.3.0

First follow the [runtime build instructions](../docs/BUILDING.md), including
its component checks. Use this candidate's source tree and generated
image-location runtime. Supply your own official firmware in `inputs/`, your
Ed25519 public key in `inputs/owner.pub`, and the pinned image tool:

```sh
# Reuse a local MPC-LiveXplore clone if you already have one.
sh firmware/prepare-upstream.sh /absolute/path/to/MPC-LiveXplore
```

Use Go 1.26.4 for the plugin admission helper. With the matching SD bundle
available as a regular file, run from the repository root:

```sh
./vst2/pd-rack-prototype/compose.sh /usr/share/mpclearn/mcu
# Use the stage it prints as PD_RACK_HOST_STAGE=...
bash firmware/build-prerelease.sh "$PD_RACK_HOST_STAGE" \
  /absolute/path/to/MPCLEARN/plugins.squashfs build/v0_3_0
```

The stage holds the release runtime with the Pd and Cardinal touch editors;
the plain component-test build lacks them and is refused.

The output is
`build/v0_3_0/MPC-3.9.1-Gen1-CE-v0.3.0-update.img`, its payload and
verification report, and the matching `MPCLEARN` folder. This is a local image
for the owner's use; do not redistribute Akai firmware.

The bundle must be 146,313,216 bytes, SHA256
`5338999914bad437c05542e1abb13392f7d994351f8b50e2e21eb276d72e0c4a`.
The build checks this identity and the eleven-file prerelease runtime manifest
before assembling the image. It refuses changed runtime bytes; do not repin
an unqualified build to force admission. Building does not qualify flashing,
reboot, controller hardware or audio behavior.

`package.py` accepts only the qualified native Preferences observer and matched
runtime package. It intentionally refuses an arbitrary unqualified runtime.
The production builder uses the exact stock MPC executable and its separately
patched CE executable, targets `/usr/share/mpclearn/mcu`, and gives the package
the required manual lifetime.

The image builder reuses TheKikGen's pinned image tool at
`9be3e63bf03d3467cf06478b48671894dcc634f4`, then independently decodes the output.
See `upstream/COMMIT`. A source release must resolve that tool's distribution
license or require users to obtain it from upstream; do not silently relicense it.

Generated files stay in the chosen build output; the USB image name retains
`-update.img`.
Verification checks the exact official input, XZ/SHA1 image structure, geometry,
filesystem consistency, full semantic delta, each payload byte/mode/owner and
the exact CE label and Az01 EHCI CPU slots in `/usr/bin/MPC`, with its stock
metadata preserved. `mpc-ce.json` pins both intervals and the complete input/output
identities. The Az01 policy word selects CPU0 instead of CPU2 for EHCI servicing;
it changes no worker affinity or priority. This remains a prerelease candidate
qualified on the tested Live II workload, not a guarantee for every load. Building
and checking an image does not prove it flashed or booted.

### Browser patch variants

The checked-in `resources/310` recipes match v0.4.0; the original recipe
directories retain v0.3.0 for 3.9.1. Rebuilding the browser does not require
regenerating either set. The following describes their root and patch inputs; keep
all variants matched when preparing another qualified candidate.

Maintainers prepare the SSH-enabled reference root with the canonical 81-byte
Ed25519 placeholder instead of a real owner's key. `make-patch.py` emits only
changed byte spans and verifies exact reconstruction. For the default variant,
`disable-ssh.py` removes `authorized_keys` and the SSH boot link, and replaces
the `sshd.service` unit with a `/dev/null` mask. Run the filesystem check and
compare its inventory: only those three paths may differ from the enabled root.
Generate that recipe with `make-patch.py --no-ssh`; its manifest must declare
`ssh_enabled: false`, `key_offset: -1`, and an empty `key_placeholder`.

The browser embeds the two normal verified recipes. No selected key means the
disabled recipe. A valid selected Ed25519 public key selects the enabled recipe,
whose placeholder is replaced locally before the final image verification.

The separate USB diagnostics choice is a third no-SSH recipe. It overlays the
exact current no-SSH root with one bounded current-report ARMhf collector, an independent
systemd unit and its `multi-user.target.wants` link. It does not require
rebuilding the native runtime. The historical
`build-diagnostic-root.sh` helper expects the v0.2.6-style root layout and
`build/fs_inventory`; for another output layout, use `diagnostics-patch.py`,
`diagnostics-verify.py` and `make-patch.py --no-ssh` with its matching root paths.

`diagnostics-verify.py` compares every pre-existing filesystem inventory entry
with the current root and checks the three added paths, modes, bytes and unit
contract. `diagnostics-test.py` exercises the bounded report/export failure
fixtures in Linux. A native MPC flash/reboot is still required to qualify boot,
input, process-memory and USB behavior.

## First boot and rollback

The runtime runs in place from `/usr/share/mpclearn/mcu`. The image sets its
modes (executables 0700, observer, `config.h` and manifest 0600, folder 0700);
`verify.py` asserts them by name and the browser recipes carry the same inode
bytes. The boot unit starts the session unless `/data/mpclearn/disabled` exists.

An early unit runs `provision.sh` on every boot, after writable `/data` and
before the session unit. It checks the payload once per image revision and
prepares exactly one folder, `/data/mpclearn` (root, 0700), with `session/`,
`history/` and the `image` marker. It uses an existing `/data/mpclearn` only if
it is a root-owned 0700 folder; anything else there is refused with a message
and nothing is changed. It reads, writes and deletes nothing else. A
development override in `/data/mpclearn/dev` is used only while it matches the
image; otherwise it is ignored and left in place.

Reinstalling the official firmware restores its root filesystem and leaves
`/data/mpclearn` (and, on SSH images, `/data/ssh/mpclearn`) behind. Use the
[startup/recovery guide](../controllers/program-observer/MCU-START.md) for
normal disable/start/stop.

`provision-check.py` runs the image's provisioner and runtime with the image's
own ARM userland in an extracted root:

```sh
docker run --rm --network none --tmpfs /provision-root/run -v "$PWD:/work" \
  mpclearn-build:local python3 firmware/provision-check.py build/rootfs.final.ext
```

Add `--recipe DIR` to check the root that a browser recipe produces from the
official root instead, and `--downgrade OLDER_ROOT` to also run an earlier
release's provisioner beside `/data/mpclearn`.

**MIDI setup:** MCU uses native MPC commands and does not require Global MIDI
Learn, a selected XMM profile or the virtual adapter's Control input. In MPC
Preferences > MIDI/Sync, disable Track, Global and Control for X-TOUCH_INT to
prevent raw MCU messages from playing instrument notes or bending pitch. Leave
other MIDI ports configured as usual. The image preserves user settings.
Global MIDI Learn and the optional Mini/Launch Control mappings remain a
separate feature; the MCU image does not install a learned profile.
See the [release notes](../docs/releases/v0.2.6.md) for known limits.

## Offline 3.10 port discovery

`mpc-3.10.json` records the official Gen1 image, decoded root filesystem and
`/usr/bin/MPC` identity. Existing production defaults and runtime admission still
use 3.9.1; the explicit offline preparation paths below use these stock pins. With the local stock executables available:

```sh
python3 scripts/firmware_reanchor.py \
  --old build/MPC --new build/mpc310/MPC \
  --image inputs/MPC-3.10.0-Gen1-update.img \
  --out /tmp/mpc310-worklist.json
python3 scripts/firmware_reanchor_test.py
sh controllers/program-observer/plugin-host-probe-component-check.sh \
  build/mpc310/MPC build/mpc310/MPC /tmp/mpc310-guard --offline-3.10
```

The worklist includes source/line locators and source hashes for model hooks,
native Preferences, the private UI profile and VST preparation scripts. Counts
separate source occurrences from distinct coordinates, detours, ranges and other
addresses. A script with zero numeric ELF coordinates remains in the coverage
list. Computed addresses and runtime object fields outside these inputs are not
exhaustively inventoried; profile size/constant definitions are listed unresolved.

The matcher uses ELF virtual addresses, PREL31 `.ARM.exidx` ranges and Itanium
RTTI names, primary/secondary vtables and slots. The existing model preparation
script's unwind decoding and the ignored `build/mpc310-survey/tools` were starting
references; the worklist is recomputed from pinned binaries, not survey output.
Only a unique full unwind-body match of at least 16 bytes gives an `auto`
address suggestion. Normalization masks direct ARM branch displacements and
file-backed pointer literals, so a normalized match still requires branch/ABI
review. Conflicting RTTI evidence, duplicate or short bodies, changed functions,
data tables and ranges crossing unwind boundaries remain `needs-trace`. A
changed function's RTTI owner candidate does not imply a valid interior offset.
No suggestion validates object layouts or updates any runtime profile.

The small proof is the finite plugin-host diagnostic guard, prepared through
its existing script with explicit `--offline-3.10`. The 3.10 coordinates follow
the LinuxFb type's native-handle slot, the unchanged getWindowHandle/getPeer
bodies, and openPluginWindow's two calls to getWindowHandle. Its first call at
`0x008fd364` is followed by `CMP r0,#0`; the admitted body is
`[0x008fd2e8,0x008fd624)`. The native-handle leaf calls `abort@plt`. The generator
pins all seven ranges, checks relocated vtable targets and rejects a direct
branch into the displaced comparison. Its 3.10 header refuses compilation
outside the component fixture.

The focused check compiles and runs the existing ARM component with that header:
real admission, pointer mismatch rejection, patch rollback and preserved ARM
context. The fixture uses a nonzero image bias, exact-address mappings and a
separate executable address range. Component objects, getPeer, peer and the MPC
gesture are substitutes. This does not run the full
`plugin-host-probe-check.sh` observer composition/reader/dependency checks, native
MPC code, controller workflows or audio, and it does not establish 3.10 support.
The ordinary 3.9.1 component can be checked through the same focused script by
providing the CE and stock 3.9.1 files and omitting `--offline-3.10`.

### Explicit 3.10 CE data patch

The offline port also has an explicit CE v0.4.0 data patch. Existing release
commands keep their 3.9.1 defaults; this option does not admit a 3.10 runtime:

```sh
python3 firmware/mpc-310-slot-check.py build/MPC build/mpc310/MPC
python3 firmware/patch-mpc-ce.py build/mpc310/MPC /tmp/mpc310-CE --mpc-3.10
```

`mpc-ce-3.10.json` pins the complete stock/output identities and both file
intervals. The nine-character InfoTab append uses ` CE 0.4.0`; its native
constructor copies exactly nine bytes. The Az01 EHCI CPU word is at virtual
address `0x06cae110`, file offset `0x06cad110`. The slot check replays the
RTTI, constructor, table and IRQ-writer instruction evidence from the exact
stock executables. Native display and IRQ behavior still require qualification.

For a disposable copy of the decoded stock root, the same explicit option on
`patch-mpc-root.py ROOT CE_MPC DELTA_JSON --mpc-3.10` applies the two slots while
preserving the executable inode. The existing image-build container can run
`patch-mpc-test.py STOCK_MPC NEW_WORKDIR --mpc-3.10` to exercise exact bytes,
refusals and rollback against a synthetic ext filesystem. Omit the option to
check the established 3.9.1 contract. Neither check produces a release image or
qualifies the controller, editor, plugin or audio runtime.

The canonical decoder accepts `--mpc-3.10`: without a reference it requires the
exact official 3.10 image and stock root hash; with a reference it requires that
exact official reference, matching container metadata and the 3.10 root size.
`image_format_310_check.py NEW_OFFICIAL OLD_OFFICIAL NEW_WORKDIR` checks both
versions and wrong-image/root refusals. Its malformed-root image and expected
root-digest fault are test substitutions.

`firmware/build.sh` selects that path only with `MPC_FIRMWARE_VERSION=3.10`, an
explicit `MPC_CE_EXECUTABLE` and an explicit `MPC_IMAGE_OUTPUT`. It forwards the
same selection to the CE root patcher and payload verifier. The explicit
`build-310-candidate.sh` and `build-310-recipes.sh` wrappers own the 3.10 payload,
image and three recipes. The original prerelease wrapper keeps
its frozen 3.9.1 metadata and checksum contract. Building does not establish
release qualification; consult `release/public-source.json` for that status.
