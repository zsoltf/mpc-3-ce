# MPC host-widget UI toolkit

This library provides the native controls used by CE's Controllers preferences,
Global MIDI Learn editor and Pd/Cardinal editor integration. It supports the
MPC 3.10 build used by CE v0.4.3 and the retained MPC 3.9.1 build used by CE
v0.3.0. Host integration is tied to the exact MPC executable for each build;
it is not a general UI library for arbitrary firmware or VST plugins.

## Source and capabilities

Screen code includes [mpclearn-ui.h](include/mpclearn-ui.h). The API provides:

- Buttons, labels, horizontal and vertical sliders, and scrollable lists.
- Absolute and grid layout, visibility, selection and silent value updates.
- Native keyboard dialogs through `mpc_ui_text_open`.
- An opaque RGBA image surface with pointer and contact callbacks.
- List drag-out gestures for editor palettes.
- App open, suspend/resume and deferred close handling through a host adapter.

[mpclearn-ui.cc](mpclearn-ui.cc) implements the toolkit. Firmware-specific
addresses and layouts live under [private/](private/). The toolkit uses MPC's
existing widgets; it does not link another JUCE runtime or create a service,
thread or persistent state store.

The [Global MIDI Learn screen](screens/global-midi-learn-screen.cc) shows
functional groups, targets and assignments, with mapping selection, Learn,
Clear, manual assignment, New, Copy, Import and Export controls. Its adapter
uses MPC's existing mapping owner for edits and persistence. Native Copy and
Export can discard desktop-only targets or metadata; retain original desktop
mapping files if those fields matter.

The [Pd/Cardinal integration](../vst2/pd-rack-prototype/) connects the editors
to MPC's instruments, inserts and Q-Links. Its folder name is historical;
this is part of the shipped plugin integration. A new consumer still needs
its own adapter for attachment, navigation and lifetime management.

## Build

Use the compiler and Docker environment described in
[Build from source](../docs/BUILDING.md). From the repository root:

```sh
docker build -t mpclearn-controls-build controllers
MPCLEARN_FIRMWARE=3.10 ./ui/build.sh
MPCLEARN_FIRMWARE=3.10 ./ui/check.sh
```

Use `MPCLEARN_FIRMWARE=3.9.1` for the retained profile. Outputs under ignored
`ui/package/` include `libmpclearn-ui.a`, example and screen objects, and the
component executable. Component checks use simulated host widgets and input;
they do not run the MPC application or establish audio performance.

The complete runtime build selects and compiles the toolkit with the matching
host adapter. Follow the [MPC 3.10 runtime instructions](../docs/BUILDING.md#mpc-310-candidate-runtime)
for that composition. The library alone does not register an app or attach a
plugin editor.

## Writing a screen

[toolkit-example.cc](examples/toolkit-example.cc) demonstrates buttons, labels,
sliders and app callbacks. These examples are developer samples, not additional
apps installed by CE. The [native UI guide](../docs/native-ui-guide.md)
contains adapter details; the public header documents individual API contracts.

1. Receive opaque context, parent and host handles from the firmware adapter
   on its UI thread. Screen code should not copy firmware addresses or layouts.
2. Keep product state with its existing owner. Callbacks request changes;
   refresh labels, values and selection silently from that owner's state.
3. Keep callback owners and borrowed list data alive until the screen closes.
4. Request Back with `mpc_ui_app_request_close`. Do not destroy a screen from
   its own active callback; the adapter completes closure after it returns.
5. Close the screen before destroying its parent or context. If detaching a
   child fails, retain it for retry rather than freeing a host-owned object.

Image surfaces use fixed dimensions up to 1280 by 800 and opaque RGBA pixels.
Update only when pixels change, or use invalidation and a refresh callback.
Hide, suspension and close cancel active contacts; consumers must cancel any
pending engine edit rather than treating cancellation as a release.

Firmware profiles are selected at build time. The adapter must verify the
matching executable before creating a context; selecting a version name alone
is insufficient. General dropdown widgets, a public arbitrary Graphics API and
a general UI scheduler are not provided.
