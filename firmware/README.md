# MPC 3 CE firmware

CE v0.4.3 uses MPC 3.10 Gen1. The browser builder also retains CE v0.3.0
for MPC 3.9.1 Gen1. MPC Live II is the tested model; other Gen1 models are
untested. Gen2 and Force are not supported.

For installation, use the [image builder](https://mpc3ce.com/builder.html)
and the [installation instructions](../README.md#install). The builder creates
a personal image from the official Akai update you supply. Firmware images
containing Akai's software are not distributed by this repository.

## What the image contains

The CE image adds the controller runtime, Controllers preferences, Global MIDI
Learn editor, mouse and wheel editing, and integration for the optional Pd and
Cardinal plugins. The runtime runs from `/usr/share/mpclearn/mcu` and starts
automatically. See the [release notes](../docs/releases/v0.4.3.md) for features
and known limits.

Pd Sketch Synth, Pd Sketch FX, Cardinal Synth and Cardinal FX also need the
[SD plugin bundle](plugin-setup/README.md). The same v0.4.0 bundle works with
CE v0.4.3. Keep its SD card inserted while using the plugins.

SSH is disabled by default. Selecting your own Ed25519 public key in the
builder enables root login with that key. Private keys are never needed by
the builder. Each MPC creates its own persistent SSH host keys; password
login is disabled. Do not share a personalized firmware image.

## Build from source

Follow [Build from source](../docs/BUILDING.md#mpc-310-candidate-runtime) for
MPC 3.10 extraction, runtime composition and image packaging. That workflow
includes the Pd/Cardinal editors and GPU presenter. Building the
[browser site](../builder/README.md) uses the checked-in patch recipes and does
not require rebuilding the runtime.

[release/public-source.json](../release/public-source.json) identifies the
v0.4.3 runtime, payload checksums and SSH, no-SSH and Debug recipes. Its image
revision is `v0_4_3`, runtime format is CMD32/MMV24, and installed Info label is
`MPC 3 CE 0.4.3`. The runtime's `BUILD.json` records its original build; later
documentation updates do not change those release bytes.

The retained 3.9.1 build has separate
[metadata](../release/public-source-v0.3.0.json) and
[build instructions](../docs/BUILDING.md#mpc-391-runtime-and-historical-v030-build).
Older packaging scripts and manifests remain for reproducing those builds;
use the instructions for the firmware version you intend to build.

Maintainers should follow [Publishing](../docs/PUBLISHING.md) when preparing
a new release. Building an image alone does not establish hardware or audio
compatibility.

### Image packaging prerequisites

Image packaging requires Go 1.26.4, the Docker build environment above,
your official update in `inputs/`, and your Ed25519 public key at
`inputs/owner.pub`. Prepare the pinned upstream image tool before packaging:

```sh
# Reuse an existing clone if available.
git clone https://github.com/TheKikGen/MPC-LiveXplore.git ../MPC-LiveXplore
sh firmware/prepare-upstream.sh ../MPC-LiveXplore
```

For MPC 3.10, continue with `build-310-candidate.sh` and
`build-310-recipes.sh` as described in the linked build instructions.

### Retained v0.3.0 image packaging

Use the matching v0.3.0 source and its runtime pins to reproduce that release.
After the 3.9.1 runtime composition, use its printed `PD_RACK_HOST_STAGE` and
the matching SD bundle:

```sh
bash firmware/build-prerelease.sh "$PD_RACK_HOST_STAGE" \
  /absolute/path/to/MPCLEARN/plugins.squashfs build/v0_3_0
```

The output directory must be new. The wrapper checks the bundle against
`firmware/prerelease-bundle.json` and the runtime against
`firmware/prerelease-runtime.sha256`. It produces the personal update image,
payload and SD folder under `build/v0_3_0`; do not replace those pins merely
to accept a different build.

## Startup and recovery

The boot service starts the controller session unless `/data/mpclearn/disabled`
exists. Image provisioning prepares `/data/mpclearn` for runtime state.
A development override is used only while it matches the installed image.

Reinstalling official firmware restores the stock root filesystem but leaves
CE's persistent data directories behind. See the
[startup and recovery guide](../controllers/program-observer/MCU-START.md)
for disabling or restarting the session and returning to official firmware.

Controller MIDI routing is documented in the
[controller setup guide](../docs/controller-programs.md). Native controller
support does not require a Global MIDI Learn mapping.
