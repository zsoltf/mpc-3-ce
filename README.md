# MPC 3 Community Edition

MPC 3.9.1 with controller feedback, clip and drum sequencing, touchscreen Pd and
Cardinal, USB mouse support and easier note selection. Build your own image in
your browser from the official Akai update. Firmware stays on your computer.

**[Website](https://zsoltf.github.io/mpc-3-community-edition/)** |
**[Image builder](https://zsoltf.github.io/mpc-3-community-edition/builder.html)** |
**[Controller setup](docs/controller-programs.md)**

## What's included

- Full-size X-Touch: motor faders, names, meters, mixer, sends, inserts, Q-Links,
  transport and jog controls. See the [X-Touch guide](https://zsoltf.github.io/mpc-3-community-edition/guide.html).
- Launchpad X and Pro Mk3: clips, scenes, Arrangement and drum-step editing.
  Two Launchpads can work on clips and drums together.
- Launch Control XL3: track and drum-pad mixing, sends, Q-Links and effect
  controls. Play starts, pauses and resumes, with matching feedback.
- More MCU/HUI and Akai profiles, with explicit mode and program setup.
  Hardware coverage varies; a profile does not mean every device is tested.
- Pd Sketch Synth/FX and Cardinal Synth/FX: touchscreen patch editing and
  project recall, installed by copying the matching plugin bundle to an SD card.
- Native controller preferences and Global MIDI Learn, USB mouse editing,
  and Shift + data-wheel note selection in the Grid.

## Install v0.3.0

MPC Live II with the official **3.9.1 Gen1** update is the tested device.
Other Gen1 models are untested; Gen2 and Force are unsupported.

1. Back up your projects and keep the official Akai update for recovery.
2. Open the [image builder](https://zsoltf.github.io/mpc-3-community-edition/builder.html)
   and select `MPC-3.9.1-Gen1-update.img`. SSH is optional; supply your own public
   key if you want it.
3. Copy the generated `*-update.img` to the root of a USB drive. Keep its name.
   Connect AC power and use Preferences > Info > Update > USB Drive Update.
4. Download `mpc-ce-v0.3.0-sd-plugins.zip` from the
   [matching release](https://github.com/zsoltf/mpc-3-community-edition/releases/tag/v0.3.0)
   and unzip it. With the MPC off, copy its `MPCLEARN` folder to the root
   of your SD card. Insert it before startup and leave it in while using plugins.
5. Set your controller's mode and MPC MIDI routing using the
   [controller guide](docs/controller-programs.md). For X-Touch, use MC/USB and
   turn Track, Global and Control off for `X-TOUCH_INT` in MIDI/Sync.

Preferences > Info retains the `MPC CE v0.2.6` base label. Use the v0.3.0 filename
and checksums to identify this release. Settings and projects are kept.

## Known limits

Cardinal's MIDI and Terrorform polyphony is limited to four voices. Demanding
patches can still overload audio; simplify the patch or reduce polyphony. Avoid
the Waves example. Named Cardinal host-parameter controls are not included.

Some compact MCU/HUI and Akai workflows remain unverified. Wider reconnect,
concurrent-controller and long-session audio coverage is incomplete. Mouse
right-click and long-press gestures are unavailable. See the
[release notes](docs/releases/v0.3.0.md) for controller-specific limits.

## Source and support

[Build from source](docs/BUILDING.md) |
[Report a bug](https://github.com/zsoltf/mpc-3-community-edition/issues) |
[Recovery](controllers/program-observer/MCU-START.md)

Built on [MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
[MIT license](LICENSE), with [third-party notices](builder/THIRD_PARTY_NOTICES.md).
Unofficial; not affiliated with Akai, inMusic, Behringer or Novation.
Do not redistribute generated Akai firmware images.
