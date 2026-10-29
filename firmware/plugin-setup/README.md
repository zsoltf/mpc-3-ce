# Pd and Cardinal for MPC

Adds Pd Sketch Synth and Cardinal Synth as instruments, and Pd Sketch FX and
Cardinal FX as insert effects. Cardinal supports GPU panning. Existing
controller support and Global MIDI Learn remain available.

Use `mpc-ce-v0.4.3-sd-plugins.zip` from the
[v0.4.3 release](https://github.com/zsoltf/mpc-3-ce/releases/tag/v0.4.3)
with CE v0.4.x on MPC 3.10. The plugin bundle is unchanged
between these releases. For the CE v0.3.0 image on MPC 3.9.1,
use its [matching release](https://github.com/zsoltf/mpc-3-ce/releases/tag/v0.3.0).

1. With the MPC turned off, copy the ZIP's `MPCLEARN` folder to the root of your
   SD card. It contains `plugins.squashfs`.
2. Build your image from your official Akai update with the
   [image builder](https://mpc3ce.com/builder.html), then install it through
   Preferences > Info > Update. The plugin ZIP contains no firmware image.
3. Insert the SD card before starting the MPC and leave it inserted while using
   Pd or Cardinal. Turn the MPC off before replacing the plugin bundle.

No SSH is needed. If the SD bundle is missing or does not match the firmware,
the base MPC and controller session still starts, but Pd and Cardinal are
unavailable. After installing v0.4.3, Preferences > Info shows `MPC 3 CE 0.4.3`.

Cardinal MIDI and Terrorform support up to four voices. Heavy patches and
concurrent editing can still overload playback; reduce polyphony or simplify
the patch. Avoid the Waves example. Named Cardinal host-parameter controls
are not included.
