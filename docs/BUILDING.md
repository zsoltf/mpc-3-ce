# Build from source

The website and native MCU runtime have separate builds. End users need only
[the browser builder](https://mpc3ce.com/builder.html).

## Website

Install Go 1.26.4 (the release compiler), then from the repository root:

```sh
cd builder
go test ./...
./build-web.sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory dist
```

Open http://127.0.0.1:8765/ for the landing page and
http://127.0.0.1:8765/builder.html for the image builder. Deploy only
`builder/dist/`. The checked-in patch recipes already contain the matched
runtime; building the website does not require firmware, SSH keys, Docker or
rebuilding the native runtime.

## MPC 3.10 candidate runtime

The v0.4.2 candidate selects MPC 3.10 explicitly. Put the official Gen1 USB
image at `inputs/MPC-3.10.0-Gen1-update.img`. With the Docker images described
below available, extract the stock executable and compose the complete runtime:

```sh
mkdir -p build/mpc310
docker run --rm --network none -v "$PWD:/work" -w /work mpclearn-build:local sh -ec '
python3 scripts/image_format.py inputs/MPC-3.10.0-Gen1-update.img build/mpc310/rootfs.original.ext --mpc-3.10
debugfs -R "dump /usr/bin/MPC /work/build/mpc310/MPC" build/mpc310/rootfs.original.ext
'
MPCLEARN_FIRMWARE=3.10 ./vst2/pd-rack-prototype/compose.sh /usr/share/mpclearn/mcu
```

The stock MPC SHA-256 is
`753f838ca362d42c8cc3f5c9c3fcc604ba076b5919212e0301923a3c11c93494`.
This works from an unpacked source snapshot without Git. The printed host
stage contains the observer, Pd/Cardinal editors, Cardinal GPU presenter and
`source-inputs.json`. The accepted SD plugin bundle is unchanged.

For a private candidate image from the development checkout, pass that host
stage, the accepted `plugins.squashfs`, a new output directory and the full
source commit to `firmware/build-310-candidate.sh`. Packaging checks the composed
source against the commit and writes the actual protocol/version in `BUILD.json`.
Generate its SSH, no-SSH and Debug browser recipes with
`firmware/build-310-recipes.sh OUTPUT_DIRECTORY`.
A source-only local export uses `scripts/export_public.py --candidate-310 COMMIT
--archive-directory NEW_DIRECTORY`; it does not publish anything. For a release
snapshot, use the normal export described in [Publishing](PUBLISHING.md); the
candidate option intentionally resets qualification metadata. The runtime's
`BUILD.json` remains its original build-time record. Subsequent qualification is
recorded in `release/public-source.json` against the unchanged payload and recipes.
The v0.4.2 checksum files identify that payload; the older prerelease checksums and
`release/public-source-v0.3.0.json` remain paired with the 3.9.1 packaging path.

## MPC 3.9.1 runtime and historical v0.3.0 build

Use Docker, Python 3, Clang with ARM cross-target support, and an official MPC
3.9.1 Gen1 USB image. The tested host compiler is Apple Clang 17.0.0; the Docker
image supplies the ARM GCC linker, headers and libraries. On Linux, running
ARM component tests also requires configured ARM user-mode emulation; Docker
Desktop's Linux VM supplies this on the tested development platform.

From the repository root, put the official download at
`inputs/MPC-3.9.1-Gen1-update.img`, then extract your own MPC executable:

```sh
mkdir -p inputs build
docker build -t mpclearn-build:local .
docker build -t mpclearn-controls-build controllers
docker build -t mpclearn-editor-build vst2/pd-rack-prototype
docker run --rm --network none -v "$PWD:/work" mpclearn-build:local sh -ec '
python3 scripts/image_format.py inputs/MPC-3.9.1-Gen1-update.img build/rootfs.original.ext
debugfs -R "dump /usr/bin/MPC /work/build/MPC" build/rootfs.original.ext
'
```

Expected official image SHA-256:
`4c0f7797a006313fad206f9513eca8b3823e834c9e0941a6b5f088386cb835d6`.
Expected extracted MPC SHA-256:
`bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426`.
Preparation rejects a different executable or unexpected native instructions.

Create the separate CE executable; this keeps the pristine extraction intact:

```sh
python3 firmware/patch-mpc-ce.py build/MPC build/v0_2_6/MPC
```

The output must be 112,222,004 bytes with SHA-256
`2fa5e9818c3a3beedfcb3fafe34f5b3f9a07295a3b68fb9e7949be6b64fa8daf`.

Build and run the release component tests:

```sh
./controllers/program-observer/native-preferences-build.sh \
  "$PWD/build/v0_2_6/MPC" "$PWD/build/MPC" /usr/share/mpclearn/mcu
./ui/build.sh
./ui/check.sh
./controllers/program-observer/native-preferences-check.sh
./controllers/program-observer/mirror-input-check.sh
./controllers/program-observer/location-check.sh
```

These ARM tests exercise the canonical `ui/` toolkit, its public-only third and
Global MIDI Learn assignment screens, the production native UI adapters,
observer, command mailbox and
controller logic with component substitutes. They do not render or dispatch
touch in MPC, run an MPC musical project, or prove hardware/audio acceptance.
See the [native UI recipe](native-ui-guide.md) and
[toolkit guide](../ui/README.md) for the exact host boundary and remaining UI
limits. This component-test build writes its package to
`controllers/program-observer/package/native-preferences/runtime/`, without the
Pd and Cardinal touch editors.

Build the release runtime, with the touch editors, from this checkout or an
unpacked source ZIP:

```sh
./vst2/pd-rack-prototype/compose.sh /usr/share/mpclearn/mcu
```

It copies the source tree into a new stage under `build/pd-rack-prototype/`
(printed as `PD_RACK_HOST_STAGE`), composes the Pd and Cardinal editors into the
observer, derives the CE executable from `build/MPC` and builds there. The
package is `$PD_RACK_HOST_STAGE/controllers/program-observer/package/native-preferences/runtime/`.
For v0.3.0 its `command-observer.so` SHA-256 is
`add7dd9523d5eb8c5453861896d2909eeb70e79f3deb4ec8829f460cb8dc02ef` and every
file matches `firmware/prerelease-runtime.sha256`; `firmware/package.py
--prerelease` refuses an observer without the editors.
`/usr/share/mpclearn/mcu` is where the image runs the package from. To create
an image of the tested release, use the canonical firmware build; see
[firmware preparation](../firmware/README.md).

### Trying a build on your own MPC

A development build has exactly one place on the device: the override folder
`/data/mpclearn/dev`. The build scripts refuse any other folder, so an
experiment cannot leave copies, backups or staging folders behind. This needs
an image made with your own SSH key.

```sh
./vst2/pd-rack-prototype/compose.sh /data/mpclearn/dev
```

Before staging or restarting, measure available blocks on `/data` and
`/media/az01-internal/Settings/MPC`, as well as `/tmp`. `/` is a different
filesystem and does not establish room for the development override, session
receipts or native settings. The installer temporarily keeps both runtimes.
The next start also archives the prior session. Native filmstrip generation can
consume space after startup, so remeasure before stopping; a zero-availability
data partition has prevented the session owner from writing its revocation.

Copy the printed stage's `controllers/program-observer/package/native-preferences/runtime/`
to a temporary folder on the MPC, such as `/tmp/mcu-dev`. To keep the SD
plugins mounted, also copy `plugin-setup` and `plugin-bundle.json` from
`/usr/share/mpclearn/mcu` into it and append their `sha256sum` lines to its
`session-package.sha256`. Then on the MPC:

```sh
/usr/share/mpclearn/mcu/mcu-boot-install.sh disable
/usr/share/mpclearn/mcu/mcu-boot-install.sh override /tmp/mcu-dev
rm -rf /tmp/mcu-dev
/usr/share/mpclearn/mcu/mcu-boot-install.sh enable
reboot
```

`disable` stops the running session and restores the stock app first; save the
project before it. The installer checks the package and sets root ownership and
the shipped modes.
The override runs only on the image it was installed for; with a different
image it is ignored and the image runtime runs. Remove it yourself with
`/usr/share/mpclearn/mcu/mcu-boot-install.sh override clear`. An override is
built for the override folder, so the exact release bytes are tested only by
flashing an image.

`firmware/package.py` is a release qualification gate, not a general installer.
For v0.3.0, its `--prerelease` mode accepts only the eleven files pinned by
`firmware/prerelease-runtime.sha256`, built for the image location with the
manual lifetime. The canonical [firmware build](../firmware/README.md) adds
the plugin admission helper and matching bundle descriptor before this check.
The default mode and `firmware/runtime.sha256` retain the frozen v0.2.6 pins.
A changed compiler or source may produce different bytes. Such a build is a
new candidate, not the tested release. Updating the pins for your own build is
how you run it (see [Using your own plugin build](#using-your-own-plugin-build));
it does not make that build the tested release. Native lifecycle, controller
and playback tests are required before qualifying a changed runtime.

## Cardinal and Pd plugins

The SD bundle's Cardinal Synth/FX and Pd plugins are built from pinned upstream
source plus the scripts in `vst2/`. `release/sources/` lists every upstream
revision, the Debian source packages and the bundled libraries they bind.
These steps need Docker and network access; run them from the repository root.

Cardinal: download the pinned source closure from a Cardinal clone, apply the
CE changes, then build in the ARM container:

```sh
python3 vst2/cardinal-touch-prototype/fetch-full-source.py /path/to/Cardinal build/cardinal-full-prototype
for step in prepare-full prepare-full-owner prepare-full-paths; do
  python3 vst2/cardinal-touch-prototype/$step.py build/cardinal-full-prototype/source
done
docker build -t mpclearn-cardinal-render-probe-build vst2/cardinal-touch-prototype
docker build -f vst2/cardinal-touch-prototype/Dockerfile.rack -t mpclearn-cardinal-rack-probe-build vst2/cardinal-touch-prototype
docker build -f vst2/cardinal-touch-prototype/Dockerfile.full -t mpclearn-cardinal-full-build vst2/cardinal-touch-prototype
docker run --rm -v "$PWD/build/cardinal-full-prototype:/work" -v "$PWD/vst2:/kit:ro" \
  mpclearn-cardinal-full-build bash /kit/cardinal-touch-prototype/full-build-in-container.sh
./vst2/cardinal-touch-prototype/full-package.sh "$PD_RACK_HOST_STAGE"
```

The Cardinal clone must contain the pinned commit. `full-package.sh` strips the
plugins and copies the bundled Debian libraries into `Cardinal.vst/deps`.

Pd: `vst2/pd-durm/build.sh` applies `libpd-embed.patch` to the pinned libpd and
Pure Data and builds `libpd.a`. Then:

```sh
python3 vst2/cardinal-touch-prototype/fetch-rack-source.py build/cardinal-touch-prototype/sources
./vst2/pd-touch-prototype/hosted-build.sh
./vst2/pd-touch-prototype/hosted-package.sh "$PD_RACK_HOST_STAGE"
```

`PD_RACK_HOST_STAGE` is the stage printed by `compose.sh`. The SD bundle's
`plugins.squashfs` holds `Cardinal.vst/` and `Cardinal-LICENSE` from the
Cardinal package and `pd/` with the Pd package's `plugins`, `library` and
`assets`, packed with `mksquashfs <folder> plugins.squashfs -noappend -comp gzip`.
A rebuild may not match the release binaries byte for byte.

### Using your own plugin build

Cardinal and Pd are open source, and you can run a modified build of them. Build
the plugin bundle from source as described above. The image mounts only the
bundle whose size and SHA-256 are in `firmware/prerelease-bundle.json`, so set
your bundle's values there and update the `plugin-bundle.json` line in
`firmware/prerelease-runtime.sha256`, which `firmware/package.py` checks. Then
build your own image from this source with the builder; see
[firmware preparation](../firmware/README.md).

## Source layout

- `controllers/program-observer/`: native state service, command execution,
  MCU bridge, boot/session helpers and component regressions.
- `controllers/adapter.c`, `main-button.c`: matched auxiliary tools; full MCU
  transport uses native commands, not Global MIDI Learn.
- `vst2/pd-rack-prototype/`, `vst2/pd-touch-prototype/`,
  `vst2/cardinal-touch-prototype/`: the Pd and Cardinal touch-editor sources
  that `compose.sh` builds into the release observer, and the scripts that
  build the SD bundle's Cardinal and Pd plugins; `vst2/pd-durm/` builds libpd.
- `release/sources/`: upstream revisions and Debian source packages for the
  SD bundle.
- `firmware/`: packaging, image patching, provisioning and verification.
- `builder/`: shared Go image engine, browser UI and embedded patch recipes.
- `scripts/`: independent image decoding and filesystem verification.

See [firmware preparation](../firmware/README.md) for rebuilding patch data,
[architecture](mcu-architecture.md) for ownership and
[performance](performance.md) before changing recurring runtime work.

The current toolchain can vary a temporary assembler object name in the
observer's non-loaded `.strtab` section. A different whole-file hash is a new
candidate even when loaded bytes match; do not bypass the package gate.
