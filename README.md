# MPC 3 CE

**The Controller Edition of MPC 3 for the MPC Live II.**

Motorized faders, clip launching, a step sequencer and hands-on mixing for your
MPC. Built in your browser from Akai's official update.

**[Website](https://mpc3ce.com/)** ·
**[Build your image](https://mpc3ce.com/builder.html)** ·
**[Controller guide](docs/controller-programs.md)**

## Features

- **One-press setup:** Detect finds and sets up your controllers.
- **Motorized faders:** levels, track names and meters that follow the MPC
  (Behringer X-Touch, [guide](https://mpc3ce.com/guide.html)).
- **Clip launching:** clips and scenes in your track colours (Launchpad X or Pro Mk3).
- **Step sequencer:** program drum steps while you play (Launchpad X or Pro Mk3).
- **Hands-on mixing:** tracks and drum pads on faders and knobs (Launch Control XL3).
- **MIDI Learn:** map any MIDI controller to the MPC.

## Install

Needs Akai's **MPC 3.9.1 Gen1** update. MPC 3.10 is not supported yet.

1. Back up your projects and keep Akai's update file.
2. Build your image with the [image builder](https://mpc3ce.com/builder.html).
3. Copy it to a USB drive and run Preferences > Info > Update.

## Controllers

Plug in, then press **Detect** in Preferences > CONTROLLERS. In
Preferences > MIDI/Sync, turn Track, Global and Control off for each input:

| Controller | Mode | Input |
| --- | --- | --- |
| X-Touch | MC over USB | `X-TOUCH_INT` |
| X-Touch Mini | Hold MC while connecting | `X-TOUCH MINI MIDI 1` |
| Launchpad X | Automatic | `LPX MIDI In` |
| Launchpad Pro Mk3 | Automatic | `LPProMK3 MIDI` |
| Launch Control XL3 | Automatic | `LCXL3 1 DAW In` |

## Also included

- **Pd and Cardinal:** run synths and effects on the touchscreen. To install,
  unzip [`mpc-ce-v0.3.0-sd-plugins.zip`](https://github.com/zsoltf/mpc-3-ce/releases/tag/v0.3.0)
  and copy its `MPCLEARN` folder to your SD card.
- **Mouse editing** and note stepping with Shift + data wheel.

## Limits

- Not compatible with MPC 3.10. Don't use Online Update: it replaces CE
  with stock MPC. Back up first; Akai hasn't said if 3.10 projects open in 3.9.1.
- Cardinal: four voices; heavy patches can overload audio.
- Track colour changes show after a project reload.
- Up to 12 MIDI ports on the Controllers page.
- Preferences > Info shows `v0.2.6`.
- More in the [release notes](docs/releases/v0.3.0.md).

## More

[Build from source](docs/BUILDING.md) ·
[Report a bug](https://github.com/zsoltf/mpc-3-ce/issues) ·
[Recovery](controllers/program-observer/MCU-START.md) ·
[Licence notes](NOTICE.md)

MIT licence. Built on [MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
Unofficial; not affiliated with Akai, inMusic, Behringer or Novation. Don't
share the images you build.
