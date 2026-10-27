# Pure Data in MPC 3 CE

MPC 3 CE runs **Pure Data 0.56.5**, embedded through **libpd**. The shipped
instruments are **Pd Sketch Synth** and **Pd Sketch FX**. They run Pd patches
and provide a touchscreen patch editor inside MPC.

## The actual Pure Data source

We use upstream Pure Data, rather than a separate rewritten engine. Its
source is downloaded during the build, not copied into this Git repository.
The exact revisions used are:

- [Pure Data source](https://github.com/pure-data/pure-data/tree/f009fd8d7b537e209e09898d487fdf1bf547da2b/src)
  (`f009fd8d7b537e209e09898d487fdf1bf547da2b`).
- [libpd source](https://github.com/libpd/libpd/tree/ba0dc63262901d658af8bbda5e619a60fa975e78)
  (`ba0dc63262901d658af8bbda5e619a60fa975e78`), which embeds Pd in the plugin.

[fetch-source.py](fetch-source.py) downloads these revisions and checks their
archive hashes. [libpd-embed.patch](libpd-embed.patch) prevents libpd from
changing MPC's process-wide SIGFPE signal handler when the engine initializes.

## Our MPC integration

The engine, plugin and editor are separate pieces of the published source:

| Part | Source |
| --- | --- |
| Pd/libpd source selection, embedding patch and ARM build | This directory |
| Pd Sketch Synth/FX: audio, MIDI and saved patch state | [hosted-plugin.cc](../pd-touch-prototype/hosted-plugin.cc) |
| Access to Pd objects, connections and controls | [pd-touch.c](../pd-touch-prototype/pd-touch.c) |
| Touchscreen patch editor | [pd-canvas.cc](../pd-touch-prototype/pd-canvas.cc) |
| MPC Instrument Edit, insert editor and Q-Link integration | [pd-rack-prototype](../pd-rack-prototype/) |

## Build

Start with the [plugin build instructions](../../docs/BUILDING.md#cardinal-and-pd-plugins)
for the required Docker environment and complete plugin packaging steps.
The engine build is:

```sh
./vst2/pd-engine/build.sh
```

It produces `package/armhf/libpd.a` for the MPC plugins and
`package/libpd-source.tar.gz`, containing the exact patched Pd/libpd source.
Inside that archive, the Pure Data engine is under `pure-data/src/`.
The touchscreen/plugin build consumes this library; its entry point is
[hosted-build.sh](../pd-touch-prototype/hosted-build.sh).

## Licenses

Pure Data and libpd retain their upstream [license notices](notices/).
CE's embedding patch retains the upstream BSD terms; our integration code is
covered by the repository's [license and notices](../../NOTICE.md).
