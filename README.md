# MPC 3 CE

Free, unofficial firmware for **MPC Live II**. Mix tracks, launch clips and
program drum patterns from USB controllers. Map controls with Global MIDI Learn,
edit notes with a mouse, and play Pd and Cardinal synths and effects.
Build your image in your browser from Akai's official update. Nothing is uploaded.

**[Build your image](https://mpc3ce.com/builder.html)** ·
[Website](https://mpc3ce.com/) ·
[Controller guide](docs/controller-programs.md) ·
[Release notes](docs/releases/v0.4.2.md)

## What it adds

- **Mixing:** adjust track volume and pan, select, mute, solo and arm tracks,
  and control transport. X-Touch motorized faders, track names and meters follow
  the MPC; Launch Control XL3 also controls drum-pad levels and pan.
- **Clip launching and editing:** launch clips and scenes, copy and paste clips
  or whole rows, delete clips or rows, double clips, and use Undo/Redo from a
  Launchpad. Copy the playing clips into a new row. Pro Mk3 also has a direct
  Quantise shortcut. [Launchpad guide](https://mpc3ce.com/launchpad-guide.html).
- **Drum step sequencing:** program drum patterns from a Launchpad while other
  clips play. Use two Launchpads together: launch clips on one and edit drum
  steps on the other. Each keeps its own view, and the drum editor stays on
  its chosen clip while the other Launchpad launches clips.
- **Q-Links and effects:** control Q-Links, sends and inserts from X-Touch or
  X-Touch Mini, with feedback from the MPC. Save projects from the controller.
  [X-Touch guide](https://mpc3ce.com/guide.html) ·
  [Mini guide and printable reference](https://mpc3ce.com/mini-guide.html).
- **Global MIDI Learn:** map MIDI buttons, knobs and faders to MPC functions.
- **Mouse and wheel editing:** click and drag with a USB mouse; use Shift +
  data wheel in Grid to select the next or previous note.
- **Pd and Cardinal synths and effects:** play and edit patches on the
  touchscreen with the optional SD plugin bundle.

## New in v0.4.2

Cardinal Screen Q-Links keep responding to repeated turns. Its parameters fill
columns in MPC's Q-Link grid; Mini and full X-Touch Screen banks follow the
current page's row or column direction.

## Install

CE **v0.4.2 uses MPC 3.10 Gen1**. The same builder makes CE **v0.3.0 from
MPC 3.9.1 Gen1**. Tested on MPC Live II; other Gen1 models are untested.
Gen2 and Force are not supported.

1. Back up your projects and keep Akai's official update for recovery.
2. Open the [image builder](https://mpc3ce.com/builder.html) and choose
   `MPC-3.10.0-Gen1-update.img` (or the 3.9.1 update).
3. Copy the generated image to a USB drive with the exact filename shown.
   Connect power and use **Preferences > Info > Update > USB Drive Update**.

After installing v0.4.2, Preferences > Info shows `MPC 3 CE 0.4.2`.
Don't use Online Update: it replaces CE with stock firmware.

For **Pd and Cardinal**, download and unzip
[`mpc-ce-v0.4.0-sd-plugins.zip`](https://github.com/zsoltf/mpc-3-ce/releases/tag/v0.4.0).
The same bundle works with v0.4.2. With the MPC off, copy its `MPCLEARN` folder to the SD card root and keep the
card inserted while using plugins. [Plugin installation guide](firmware/plugin-setup/README.md).

## Connect controllers

Set the mode below, then connect by USB. In **Preferences > MIDI/Sync**,
turn **Track, Global and Control** off for the listed input. Open
**Preferences > CONTROLLERS** and press **Detect**.

| Controller | Mode | MPC input |
| --- | --- | --- |
| X-Touch | MC over USB | `X-TOUCH_INT` |
| X-Touch Mini | Hold MC while connecting | `X-TOUCH MINI MIDI 1` |
| Launchpad X | Automatic | `LPX MIDI In` |
| Launchpad Pro Mk3 | Automatic | `LPProMK3 MIDI` |
| Launch Control XL3 | Automatic | `LCXL3 1 DAW In` |

Use multiple controllers together. See the [controller guide](docs/controller-programs.md)
for other profiles, routing and controls. Mouse editing and note selection
work without a controller.

## Known limits

- Back up before changing firmware versions; compatibility of 3.10 projects
  with 3.9.1 is unconfirmed.
- Cardinal supports four voices. Heavy patches can overload audio; avoid
  the Waves example.
- Track colour changes appear after a project reload. Launchpad drum steps
  use a fixed palette.
- Reassign legacy User Level Velocity mappings in Global MIDI Learn after
  moving to 3.10.
- Note selection adds native Undo history and clears Redo.
- X-Touch Shift+jog does not extend the Arrange loop; use MPC Shift+wheel.
- Further controller and plugin limits are in the [release notes](docs/releases/v0.4.2.md).

## More

[Build from source](docs/BUILDING.md) ·
[Report a bug](https://github.com/zsoltf/mpc-3-ce/issues) ·
[Recovery](controllers/program-observer/MCU-START.md#return-to-official-firmware) ·
[Licence notes](NOTICE.md)

MIT licence. Built on [MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
Unofficial; not affiliated with Akai, inMusic, Behringer or Novation.
Don't share the firmware images you build.
