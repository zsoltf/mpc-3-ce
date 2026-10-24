# MPC 3 CE

MPC 3 CE for **MPC Live II**. Add USB controller
integration, Global MIDI Learn, mouse editing, and touchscreen Pd and Cardinal.
Build your image in your browser from Akai's official update. Nothing is uploaded.

**[Build your image](https://mpc3ce.com/builder.html)** ·
[Website](https://mpc3ce.com/) ·
[Controller guide](docs/controller-programs.md) ·
[Release notes](docs/releases/v0.4.0.md)

## What it adds

- **X-Touch:** motorized faders, track names and meters that follow the MPC;
  mixer, transport, effects and Q-Link controls. [X-Touch guide](https://mpc3ce.com/guide.html).
- **Launchpad X and Pro Mk3:** launch clips and scenes, copy clips or whole rows,
  delete, double events, capture playing clips into a row and use Undo/Redo.
  Pro Mk3 also has a direct Quantise shortcut.
  Drum step sequencing works alongside clip control. [Visual guide](https://mpc3ce.com/launchpad-guide.html).
- **Launch Control XL3:** faders and knobs for tracks and drum pads.
- **Global MIDI Learn:** map MIDI buttons, knobs and faders to MPC functions.
- **Mouse and wheel editing:** click and drag with a USB mouse; use Shift +
  data wheel in Grid to select the next or previous note.
- **Pd and Cardinal:** play and edit synths and effects on the touchscreen
  with the optional SD plugin bundle.

**Two Launchpads can do different jobs.** Use a Launchpad X and Pro Mk3
together: launch clips on one while programming drum steps on the other.
Each keeps its own view, and the drum editor stays on its chosen clip while
the other Launchpad launches clips.

## Install

CE **v0.4.0 uses MPC 3.10 Gen1**. The same builder makes CE **v0.3.0 from
MPC 3.9.1 Gen1**. Tested on MPC Live II; other Gen1 models are untested.
Gen2 and Force are not supported.

1. Back up your projects and keep Akai's official update for recovery.
2. Open the [image builder](https://mpc3ce.com/builder.html) and choose
   `MPC-3.10.0-Gen1-update.img` (or the 3.9.1 update).
3. Copy the generated image to a USB drive with the exact filename shown.
   Connect power and use **Preferences > Info > Update > USB Drive Update**.

After installing v0.4.0, Preferences > Info shows `MPC 3 CE 0.4.0`.
Don't use Online Update: it replaces CE with stock firmware.

For **Pd and Cardinal**, download and unzip
[`mpc-ce-v0.4.0-sd-plugins.zip`](https://github.com/zsoltf/mpc-3-ce/releases/tag/v0.4.0).
With the MPC off, copy its `MPCLEARN` folder to the SD card root and keep the
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
- Further controller and plugin limits are in the [release notes](docs/releases/v0.4.0.md).

## More

[Build from source](docs/BUILDING.md) ·
[Report a bug](https://github.com/zsoltf/mpc-3-ce/issues) ·
[Recovery](controllers/program-observer/MCU-START.md) ·
[Licence notes](NOTICE.md)

MIT licence. Built on [MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
Unofficial; not affiliated with Akai, inMusic, Behringer or Novation.
Don't share the firmware images you build.
