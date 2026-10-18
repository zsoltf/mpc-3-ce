# Pd and Cardinal for MPC

Adds Pd Sketch Synth and Cardinal Synth as instruments, and Pd Sketch FX and
Cardinal FX as insert effects. Cardinal supports GPU panning. Existing
controller support and Global MIDI Learn remain available.

Download the matching SD plugin ZIP from the
[v0.3.0 release](https://github.com/zsoltf/mpc-3-community-edition/releases/tag/v0.3.0)
and unzip it. Use its `MPCLEARN` folder with the matching firmware image.
Do not mix the image with a plugin bundle from another release.

1. With the MPC turned off, copy the `MPCLEARN` folder to the root of your SD
   card. It contains `plugins.squashfs`.
2. Install the bundled `*-update.img` through the MPC's normal firmware update
   screen. Use the exact filename supplied in the archive.
3. Insert the SD card before starting the MPC and leave it inserted while using
   Pd or Cardinal. Turn the MPC off before replacing the plugin bundle.

No SSH is needed. If the SD bundle is missing or does not match the firmware,
the base MPC and controller session still starts, but Pd and Cardinal are
unavailable. The firmware may show its CE base label; use the bundled update
filename to identify the release.

Cardinal MIDI and Terrorform support up to four voices. Heavy patches and
concurrent editing can still overload playback; reduce polyphony or simplify
the patch. Avoid the Waves example. Named Cardinal host-parameter controls
are not included.
