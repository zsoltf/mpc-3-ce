# MCU image and release

The current candidate is `v0.3.0`, CMD31/MMV21, with provision revision
`v0_3_0`. It runs from `/usr/share/mpclearn/mcu` and uses the matching
[SD-card Pd/Cardinal bundle](plugin-setup/README.md). See the
[release notes](../docs/releases/v0.3.0.md) for features and limits.
The native Info screen retains the `MPC CE v0.2.6` base label; the update filename
and payload `BUILD.json` identify this candidate.

The current `mpc-ce.json`, prerelease runtime pins and browser recipes describe
this candidate. The default `package.py` mode, `runtime.sha256` and older
`build-v0.2.6.sh` workflow retain the historical v0.2.6 release identity; use
that workflow only from its matching frozen source.

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
bash firmware/build-prerelease.sh "$PWD" \
  /absolute/path/to/MPCLEARN/plugins.squashfs build/v0_3_0
```

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

The checked-in recipes match v0.3.0. Rebuilding the browser does not require
regenerating them. The following describes their root and patch inputs; keep
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
