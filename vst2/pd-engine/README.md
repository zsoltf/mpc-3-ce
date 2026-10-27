# Pure Data engine for MPC 3 CE

This directory builds the shared Pure Data/libpd engine used by CE's Pd
plugins. It contains the pinned upstream-source fetcher, embedding patch,
ARM build scripts and a Durm example instrument used for component checks.

## Find the source

- [Upstream Pure Data](https://github.com/pure-data/pure-data) and
  [libpd](https://github.com/libpd/libpd): original projects.
- [fetch-source.py](fetch-source.py): exact source revisions and archive hashes.
- [libpd-embed.patch](libpd-embed.patch): CE's changes to Pd/libpd embedding.
- [Touchscreen editor and Pd plugins](../pd-touch-prototype/): patch editing,
  controls and the hosted Synth/FX plugins.
- [MPC host integration](../pd-rack-prototype/): native entry points, editor
  hosting and Q-Link integration shared with Cardinal.
- [Build instructions](../../docs/BUILDING.md#cardinal-and-pd-plugins): build
  the engine, plugins and SD bundle.

The former directory name was `pd-durm`. Durm is the example instrument,
not a separate version of Pure Data. Its plugin identity and patch messages
remain unchanged. Upstream license notices are in [notices/](notices/);
CE's glue code uses the [MIT license](LICENSE).

## Durm example and component harness

This kit hosts Qu-Bit's actual asset-free Durm patch in private libpd. It has
four voices and five MIDI note-on destinations, stereo output, 19 generated
host controls, no custom editor, and complete declared musical-setting recall.
MPC integration, generated-editor access to all controls, native realtime audio,
host latency compensation and untouched MPC-project recall remain unobserved.

The VST2 instrument identity is `Pd Durm`, UID `0x50644431`, version 1,
one program, zero inputs, two outputs. The module is `PdDurm.so`. Keep the
fixed `durm.pd` beside it. The patch is resolved from the loaded module's
location and checked against its compiled SHA256 before opening; state never
contains an absolute path or media dependency.

## Build and component checks

From the repository root:

```sh
./vst2/pd-engine/build.sh
./vst2/pd-engine/check.sh
```

Docker and Python 3 are required. The digest-pinned Debian build container
installs ordinary C/ARMhf build tools and QEMU user emulation. Builds compile
private float32 static libpd with `MULTI=true`, `UTIL=false`, `EXTRA=false`,
`SETLOCALE=false`, without fast-math. ARM flags are ARMv7-A, VFPv3-D16 and
hard-float. Only `VSTPluginMain` is exported; the module links libc and libm,
with the ARMhf loader; the inspected module requires up to GLIBC_2.35. No DPF framework or Steinberg SDK is linked. The small
independent VST2 ABI follows the existing repository instrument/MVerb layout,
using `intptr_t` for pointer-sized host callback values and asserting the
144-byte ARM32 AEffect layout. DPF revision
`d4c2ce6e3da43ae328876146b60d2d2cca5caa44` is the inspected ABI comparison.

Generated `package/local/` and `package/armhf/` contain the exact module,
fixed patch and component host. `package/notices/` and `package/LICENSE` carry
the distribution notices. `package/libpd-source.tar.gz` contains the patched
Pd/libpd build source. `package/upstream-*.tar.gz` retains immutable
upstream sources; generated binaries and caches are ignored by Git.
The build scratch directory and container are removed automatically.

`check.sh` loads the actual local Linux module and then the exact ARMhf module
under QEMU. The host also links a separate private libpd for direct execution
of the same adapted patch, without the VST2 tick adapter. It checks all five
trigger destinations, all four seven-value latches and locks, fresh chunk
serialization/restore, instance isolation, note-off suppression, pending MIDI
offsets, panic and SIGFPE ownership. At 44.1 and 48 kHz it compares every PCM
sample for regular/irregular host blocks and against direct libpd with declared
timing. These are real runtime composition checks. The small host substitutes
for MPC; QEMU substitutes for native ARM hardware. Neither establishes MPC UI,
project persistence, realtime deadlines or absolute native export timing.

For root's separately authorized native component observation, stage the
contents of `package/armhf/` together and run:

```sh
/path/to/component-host /path/to/PdDurm.so /path/to
```

The checker owns no audio device and changes only its own process's SIGFPE
handler for the preservation check. Device staging and all MPC/UI actions belong
to the root, outside this local kit's observations.

## Playing and controls

| MIDI note | Voice | Original trigger receiver |
| --- | --- | --- |
| 36 | Kick | record |
| 38 | Snare | file |
| 42 | Closed hat | source |
| 46 | Open hat | reset |
| 39 | FM percussion | freeze |

Only note-on with nonzero velocity creates a hit. Velocity magnitude is not
synthesized into a new response. All MIDI channels use these same destinations.
The two hat notes share the upstream hat voice. A complete local 0/1 pulse
preserves the original `change`/`sel 1` hardware trigger logic; note-off does
not enter that graph. CC120 or CC123 invokes host panic, never `reset`.

| Indices | Host labels | Meaning |
| --- | --- | --- |
| 0-7 | Start, Size, Density, Overlap, Blend, Window, Pitch, Speed | Original shared normalized controls; voice-specific mappings are the upstream patch's synthesis/effects wiring. |
| 8-12 | St Rand, Sz Rand, Dn Rand, Ov Rand, Bl Rand | Random modulation depths for Start, Size, Density, Overlap, Blend. |
| 13 | FX Mode | Original window_alt, selecting upstream glitch/compander/filter/downsample regions. |
| 14 | K Pitch | Original pitch_alt, scaling kick pitch. |
| 15-18 | K Lock, S Lock, H Lock, F Lock | Explicit per-voice lock: Free/Locked. |

Continuous values display percent with one decimal, without quantizing stored
or DSP values. Lock parameters are boolean. Shared drum controls latch on voice
triggers when that voice is free; changing a shared control while locked does
not replace its held effective voice values. Some upstream shared controls
also affect effects continuously. Hardware button-toggle ingress is replaced
by explicit lock values driving the existing four spigots; lock processing
and random parameter latching remain in the original Pd graph. No pagination
or complete control access in MPC has yet been observed.

The synthesis is unchanged and can exceed unity: the component script observed
about 1.34 peak for kick and 1.20 for closed hat. Use host-side attenuation for
native audition/mixdown. There is no added limiter or output gain in this kit.

## Timing, lifecycle and state

Pd processes fixed 64-frame ticks. The streaming adapter preallocates its tick
and delay buffers and 1024 pending events. It declares exactly 64 frames of
VST2 buffering latency. Each MIDI event is separately rounded up to the next
Pd tick (0-63 frames). Original patch envelopes may add their own onset delay;
these are separate from buffering and quantization. Host blocks can be any
positive length, including 1, 63, 64, 257 and 511, without discarding pending
sample offsets. Zero-length calls do not advance time. Malformed MIDI or queue
overflow silences the instance and latches failure until it is recreated.

Panic cancels pending notes and upstream bursts/envelopes, clears adapter audio,
and mutes output until another trigger. DSP time may continue while muted.
Suspension also panics. Resuming does not start an internal sequencer or notes.
No patch browser, display server, canvas editor, JACK, network endpoint,
external audio process or separate PCM owner exists.

Each instance owns one Pd instance, its patch, buffers, queues and state.
Every access selects the correct Pd instance; one per-instance mutex serializes
Pd calls and coherent snapshots, including parameter updates. The audio callback
performs no file loading, patch parsing or adapter allocation. Lifecycle and
state loading must occur on the host control thread while suspended; active
SetChunk is rejected and an active sample-rate change is not applied. The host
must suspend before switching between the supported 44.1/48 kHz rates. Lock
contention and Pd's own scheduler have not been qualified for native realtime
deadlines. Host destruction must follow completion of its callbacks.

The 236-byte little-endian chunk contains magic `PDDURM1\0`, schema 1, the
32-byte SHA256 identity of the adapted fixed patch, seed-policy identifier 1,
19 normalized controls/locks and 28 actual held values (seven for each voice).
The held values are observed at the existing Pd latch outputs, not recomputed
from shared controls. Snapshots include a coherent control/held-value state.
Dedicated restore receivers update the original held-value outlets without
triggering an envelope. Bad lengths, package/schema/seed identities and invalid
numeric values are rejected before state replacement.

Restoring creates a fresh patch while suspended: explicit initialization,
controls, held values, and deterministic per-object random/noise seeds are
applied in that order without a hit. The result remains paused and silent until
host resume and subsequent note-on. Oscillator/filter/envelope/burst/timer
history and the current RNG position are volatile and deliberately excluded.
Seed policy 1 restarts all 27 random/noise objects with fixed distinct seeds on
every fresh load; it does not promise waveform continuation from a playing
instance. Identical fresh state plus identical subsequent events and idle time
produces identical PCM in the tested runtime. Rate change also recreates this
fresh history while retaining declared musical settings.

## Sources and adaptation

| Source | Revision | Downloaded archive SHA256 |
| --- | --- | --- |
| libpd/libpd | ba0dc63262901d658af8bbda5e619a60fa975e78 | 7432b9108d6741f8ca5fd786ce83ed391d92b5197e93fd3762a5e81ae663095d |
| pure-data/pure-data | f009fd8d7b537e209e09898d487fdf1bf547da2b | 29ad07f1f13dd199e2ebcd3c486dca4c496e4874dd856c925dc3891e11e58267 |
| Qu-Bit-Electronix/QB_Nebulae_V2 | 4be42b5159ae5c63466d84f0a8ab3b2fb4afd5a6 | 5d32c02655e6f6f3b0d31bf13417029caca8ed3f995c7c7fd7e6aff3de719530 |

`fetch-source.py` verifies archives before extraction. `adapt-patch.py` owns
the reproducible Durm modifications: replace network and delayed hardware
startup ingress, add deterministic seed messages, explicit locks, held-value
observation/restore, and panic messages. It preserves the voice synthesis
connections. `libpd-embed.patch` suppresses libpd_init's process-global SIGFPE
change in this private build. It never temporarily alters a host signal handler.

Durm was authored by Eli Pechman (2018) for Qu-Bit Nebulae v2. Original libpd/Pd
BSD notices and the Qu-Bit MIT notice are retained in `notices/`. This kit's
adapter code is MIT, in `LICENSE`. No patch media or external objects are needed.
