# MPC host-widget UI toolkit

## Start here: building a screen

Use the primary checkout at `/Users/zsolt/dev/codex/mpclearn`. Screen authors
include [include/mpclearn-ui.h](include/mpclearn-ui.h); they do not need firmware
addresses, object offsets or private headers. The toolkit preserves the qualified Gen1 MPC 3.9.1 host and has an explicit
3.10 development profile described below. A screen still needs an admitted host adapter;
the library alone does not register a new app or plug-in editor.

1. Start from [examples/toolkit-example.cc](examples/toolkit-example.cc) and its
   [header](examples/toolkit-example.h) for buttons, Label, Slider and app
   lifetime. Use [screens/global-midi-learn-screen.cc](screens/global-midi-learn-screen.cc)
   for grouped native lists and separate detail.
2. Keep application state in the existing owner. Callbacks emit intents;
   update text, values and selection silently from owner readback. Borrowed
   list text/model data must stay valid for the screen lifetime.
3. Receive opaque context, parent and app-host handles from the existing
   adapter on its UI thread. Register open/prepare-close/close callbacks.
   Request Back with `mpc_ui_app_request_close`; never destroy a screen on its
   own callback stack. Keep the callback owner alive until close completes.
4. Compile the screen with the canonical library into the admitted module.
   Reuse the existing page-two development example route described below.
   A different attachment owner needs separate native qualification.

For a toolkit-only build, use Python 3, Clang with ARM cross-compilation support,
Docker and the repository's controls image. From the repository root:

```sh
docker build -t mpclearn-controls-build controllers
./ui/build.sh
./ui/check.sh
```

Outputs are in ignored `ui/package/`: `libmpclearn-ui.a`, example/screen object
files and `ui-component`. Before handing source to another project, run
`python3 -B ui/check-public.py`; it builds and exercises a clean copy of the
public allow-list without publishing. These checks use host fixtures and do
not establish native rendering or audio behavior. See
[BUILDING](../docs/BUILDING.md) for firmware extraction and observer packaging,
and [the adapter guide](../docs/native-ui-guide.md) for host integration.

Buttons, Label, Slider, grouped lists, layout and app lifecycle have native
examples. General dropdowns, editable text, custom drawing, arbitrary app slots
and plug-in editor attachment are not supplied as qualified public features.
Keep new screen code at the public boundary rather than copying private ABI
recipes into it.

This directory is the canonical source toolkit for the Controllers screen,
Global MIDI Learn shell and future MPC app screens. A future plug-in UI adapter
would require its own attachment qualification. Each consumer compiles the
same [implementation](mpclearn-ui.cc) and
[public API](include/mpclearn-ui.h) into its own module. The toolkit starts no
service or thread, creates no IPC or persistent state, and links no second JUCE
runtime. Product state stays with the controller session, mapping or plug-in
parameter owner; button callbacks publish only consumer-defined intent.

The toolkit currently wraps the host's named TextButton, non-interactive Label,
horizontal or vertical Slider, selection, repaint, bounds, per-control
visibility and the qualified
keep-in-front operation. Its
private copied button table also carries the
qualified full-button painter used by the legible native Controllers work:
stored style, selection and bounds drive the admitted fill, text colour, font
and fitted-label calls. It supplies parent-local absolute and grid layout,
per-instance intent bindings, show/hide and ordered teardown. It also exposes a
narrow host ListBox with a toolkit-owned borrowed presentation model, row paint,
selection intent, native drag scrolling, empty-background styling and
owner-refresh suppression. Same-count data changes explicitly repaint the
native list. Label text and Slider values refresh silently from the consumer's
owner. Slider callbacks publish a consumer-defined intent with value-changed,
drag-started and drag-ended events; the toolkit never becomes the parameter
owner. `mpc_ui_text_open` borrows the admitted native keyboard for bounded
text entry. Arbitrary Graphics and a general UI scheduler remain outside the
public API.

## Ownership and admission

Public screens receive only opaque `MpcUiContext` and `MpcUiParent` handles.
Raw addresses, object sizes, copied vtable details and the pinned executable
identity live under [private/](private/). Each selected private profile names its exact executable SHA256 and resolves
the host functions through the existing Native Preferences admission. The
3.9.1 profile retains its native qualification; the 3.10 profile currently has
static and offline component evidence, with native qualification outstanding.

`mpc_ui_private_context_from_admitted` does not replace executable admission.
An app adapter calls it only after its existing observer admission accepts the
exact executable and provides the load bias and current UI-thread ID. A future
plug-in adapter must independently admit that same profile within the plug-in
instance after the editor-parent probe establishes its real attachment. Passing
a profile struct without that owner-side check is invalid integration.

Each context copies its own host function table. Each parent, screen, widget,
callback binding and copied button table belongs to one consumer instance. No registry or
product state is shared between consumers. Every mutating public operation and
every click require the UI thread captured at context creation; error readback
is the sole read-only exception.

The adapter creates an opaque parent with its qualified attach, detach and focus
operations. A plain admitted JUCE Component can use
`mpc_ui_private_component_parent`; it intentionally has no invented focus
operation. The default adapter treats the pinned host add/remove functions as
void and confirms each operation from the child's admitted private parent field;
no host return register is interpreted as an acknowledgement. Preferences,
app-screen and plug-in adapters remain responsible for their different parent
discovery, visibility, focus and navigation behavior.

## Authoring a screen

[examples/third-screen.cc](examples/third-screen.cc) is the public-only button
example. [examples/slider-screen.cc](examples/slider-screen.cc) adds a native
Label and Slider. [examples/toolkit-example.cc](examples/toolkit-example.cc)
combines those controls behind the public one-consumer app-host facade for the
development-only page-two qualification route. None includes a private header or contains a host address,
object offset or vtable detail. Their construction sequence is the intended
pattern:

1. Accept an admitted context and adapter parent.
2. Create one screen with bounds local to that parent.
3. Create native controls with bounded intent IDs and callbacks. Keep value
   ownership in the consumer; use silent Label or Slider updates when owner data
   changes.
4. Apply host-widget appearance, then place every control with the grid or absolute
   layout API.
5. Show the screen. The adapter receives focus only after controls are attached,
   bounded and visible.
6. Treat Back as intent for the adapter's existing navigation owner. A callback
   must not destroy its own screen before returning; close reports
   `MPC_UI_CALLBACK_ACTIVE` in that case so the adapter can close at its next
   qualified owner entry.
7. Close the screen, then destroy its parent, then destroy its context.

[screens/global-midi-learn-screen.cc](screens/global-midi-learn-screen.cc) is
the larger public-only composition used by the product adapter. Its left list
selects one of the 14 functional groups from
[screens/global-midi-learn-groups.cc](screens/global-midi-learn-groups.cc); its
middle list shows only the target names in that group. The right column shows
the selected target and readable assignment separately, with Learn/Cancel and
Back in the footer. It owns only presentation and transient selection; every
user selection or button callback publishes intent. Programmatic selection
during owner refresh is suppressed inside the toolkit, so repainting owner data
does not publish another product intent.

The same public composition reserves a separate header for the current mapping
name, previous/next selection, New, Copy, Import and Export intents. Clear
Assignment and a compact type, channel, number, control-mode and reverse draft
sit beside the selected assignment. Apply publishes that draft as one intent.
The screen still owns no file catalogue, chooser, codec, mapping or persistence
state; its adapter decides which controls are visible and resolves every intent
through MPC's existing mapping owner. The exact-firmware adapter exposes manual
Apply and Export only for a transient New or Copy that it saw MPC create in the
current screen lifetime. This keeps an arbitrary imported desktop `.xmm` out of
the stock writer's known normalization path.

At 1280 by 690, the public layout uses the full dark app surface, a 76-pixel
mapping/action header, section headings and a footer. It reserves
`(24,128,270,496)` for functional groups, `(310,128,386,496)` for target names
and `(712,128,544,496)` for selected assignment detail. Native ListBox rows
remain the only selection controls; labels provide the noninteractive headings,
values and context. Pads, Pad Banks, Q-Links, Transport / Nav, Global Controls,
Sequence / Track, Pad Performance, Looper, Step Sequencer, Editor Tabs, Modes,
Panels, Browser and Additional cover IDs 0 through 119 without gaps. The
adapter still supplies the native labels and current mapping metadata from
MPC's owner; the screen neither snapshots the whole mapping nor parses XMM
files.

Closing first invalidates every binding and removes borrowed Slider listeners,
then releases focus, hides controls and detaches children. Only after all
detaches succeed does it restore copied button vtables, destroy native widgets
and free instance allocations. A failed detach leaves an inert
screen that the adapter can retry; it never frees a child still owned by the
parent. This order protects the stale-callback and parent-owned-child failures
seen during native UI work.

This sequence is demonstrated on the app-list owner in the exact MPC 3.9.1
host. The rendered app page retained its stock layout and showed one page-two
entry. The combined public-only consumer rendered its native Label, Slider,
Action One, Action Two and Back controls. Action One refreshed the owner value
to 32 and Action Two refreshed it to 96 without publishing a Slider event. A
held native drag then published `DRAG_STARTED`, `VALUE_CHANGED` and
`DRAG_ENDED` in that order. Back closed one screen through the deferred owner
drain, and reopening constructed a fresh screen at its initial value of 64.
Repeated Main/Menu layout passes retained one entry and zero failures. The
earlier button-only third-screen observation also completed two balanced
open/close cycles. These observations demonstrate the public screen, button,
Label, Slider, layout, intent, silent owner-refresh and deferred-Back APIs on
this owner. They do not qualify another stock owner or widget class.

The Slider keeps the host widget's native vtable and
borrows one toolkit listener; close invalidates the binding, removes that
listener, detaches the widget, runs the complete host destructor and only then
frees the binding. The Label uses its inherited host renderer, disables mouse
interception and copies owner text through the host's no-notification update.
The native render, silent refresh, drag sequence, one balanced Back and fresh
reopen are observed. The later public-navigation checkpoint also suspended the
same live consumer for stock Menu, resumed the same value-32 instance from its
page-two tile, and destroyed a second suspended instance through ordinary New
Project owner teardown.
The public app-host facade now carries one consumer through that existing
qualified launcher owner. The private adapter captures context, navigation and
destructor ownership at the admitted launcher constructor hook, where the Page
is still unparented and unlaid. It creates the app host only at the admitted
completed-layout hook, after the same Page hierarchy proves its concrete local
bounds and second-page slot. Phase readback records overlay, Page, parent and
bounds at both hooks. Later layout callbacks recognize the already-owned entry
screen, control, parent and app host before the first-attachment hierarchy
guard, because adding the entry changes that native child count. A partial
owned identity fails instead of registering a duplicate. A public screen registers
open/prepare-close/close callbacks without firmware addresses. Back requests a
close and the owning UI drain runs it only after the widget callback returns.
Pending prepare-close and failed detach retain the consumer for a later drain.
When the stock Menu is opened while an idle consumer remains open, the adapter
suspends that consumer instead of closing it. The private app host snapshots
the visible state of every screen owned by the consumer parent, hides only the
shown screens, and restores only those screens when the existing launcher tile
is tapped again. Resume is queued through the same post-callback UI drain as
Back, so the launcher callback never changes screen visibility on its own
stack. A callback-active transition or host focus refusal leaves the prior
presentation intact; a partial resume is hidden again and stays suspended for
a later retry. Global MIDI Learn does not suspend while a file action, Learn
request or armed Learn state is unsettled, which keeps its native Cancel path
available. The ARM component and production composition fixtures cover this
multi-screen state preservation and refusal unwind. On MPC, Main followed by
Menu exposed the stock app list while the public example remained logically
open; tapping its existing page-two tile resumed the same instance and retained
value 32. The normal Global editor likewise retained its Copy, Q-Link 1 target
24 and unapplied channel-2 draft without another screen, target selection or
edit. These records are under
`build/native-ui-library-qualification/public-navigation-1/native/` as
`example-suspended-menu.log`, `example-resume.log`, `normal-draft.log`,
`normal-menu.log` and `normal-resume.log`.
Forced owner destruction releases the host, parent and context only after the
consumer screen detached and closed synchronously. It invalidates the separate
launcher-entry screen before attempting any close, including destruction entered
from an active callback. A failed forced detach first invalidates every callback
and then retains the complete consumer graph; after
the stock owner is gone, the adapter marks that retained graph terminal and
never drains, detaches or reopens it. This bounded failure may retain allocations
until process exit, while its live counters and toolkit failure remain visible.
The Global MIDI Learn adapter and combined public example consume this path.
Native open, Back, reopen, repeated layout and idle suspended-owner destruction
are observed. Ordinary New Project entered the real launcher-overlay destructor
with a second public consumer open and suspended; the retained old-inode state
records UI-thread and observer-running eligibility, callback depth and busy both
zero, cleanup before the stock destructor, the stock destructor's return, all
example/shell/tab/button/launcher live counts at zero and `failures=0`. The
normal Global capture records the same ordering and zero live counts. The exact
records are `example-forced-capture.log`, `example-forced-state.bin`,
`normal-forced-capture.log` and `normal-forced-state.bin` in the directory
above. Destruction entered from an active callback, with pending Learn or with a
file action remains unobserved. Dropdowns and
text entry remain library gaps.

```cpp
MpclearnThirdApp app = {};
mpclearn_third_app_init(&app, app_intent, owner);
mpc_ui_app_register(host_from_adapter, mpclearn_third_app_callbacks(), &app);
mpc_ui_app_open(host_from_adapter);
```

The screen implementation sees only `MpcUiAppHost`, `MpcUiContext`,
`MpcUiParent` and `MpcUiRect`. Its Back handler calls
`mpc_ui_app_request_close`; it never destroys itself on the active callback
stack.

The normal native observer registers Global MIDI Learn in the existing page-two
slot. A development build can register the combined public example in that same
slot without adding another tile or owner:

```sh
MPCLEARN_NATIVE_UI_EXAMPLE=1 \
  ./controllers/program-observer/native-preferences-build.sh \
  /path/to/current/MPC /path/to/stock/MPC /data/mpclearn/dev
```

The default value is `0`. The selector changes only the consumer and tile label;
it reuses the admitted launcher context, navigation parent, focus and UI drain.

## Build and composition checks

```sh
./ui/build.sh
./ui/check.sh
python3 -B ui/check-public.py
```

The build emits an ARMhf static source library, the separately compiled public
screens and a finite component executable under ignored `ui/package/`. The
component runs the actual toolkit, button and Label/Slider examples, and the
assignment screen through the ARM loader. Its host
widgets, Component relationship, Graphics calls and touch are labelled
fixtures. It checks the default void add/remove adapter against the child's
parent field, local grid bounds, full-button painter arguments, style and
selected fills, exact intent delivery, focus, binding invalidation before a
reentrant detach, retry after detach refusal, silent owner refresh, Slider
value/drag event delivery, listener removal and balanced teardown. It does not
render pixels or operate MPC.

`check-public.py` copies only `release/public-files.json` into a disposable
directory, then rebuilds and reruns the same ARM composition there. This checks
source dependency closure without changing the public checkout or publishing.

## Native acceptance boundary

The development image surface has native rendering/input and normal lifecycle
observations on the existing app-list owner. `mpc_ui_image_create` creates one
fixed-size opaque image (maximum 1280 by 800); place it at that exact
size. `mpc_ui_image_update` copies top-down RGBA with alpha 255 and a nonzero
revision, skipping a repeated revision without copying or repainting. Call it
only when pixels change. For an expensive owner renderer, register
`mpc_ui_image_on_refresh` and call `mpc_ui_image_invalidate` after meaningful
changes. Repeated invalidations request one visible paint; its refresh callback
publishes the latest pixels with `mpc_ui_image_update`. Input still reaches the
owner immediately, and publishing pixels inside that callback does not schedule
another paint. A change raised during refresh retains one later paint. Hidden
screens retain pending refresh without invoking their consumer; no idle timer
is created. The consumer must cancel engine input separately from drawing.

Pointer callbacks carry local coordinates, including
out-of-bounds drags. Hide, Menu suspension and close cancel a contact rather
than synthesize release; a stale release is ignored. Cancellation requested
inside a pointer callback is delivered after that callback returns, before the
toolkit releases its callback-depth protection. Callback owners must remain
alive through close. Existing callback-active destruction refusal is unchanged.

The optional `mpc_ui_image_contacts` callback adds stable contact IDs without
changing the single-contact callback. The consumer classifies pan/pinch and
cancels its engine's pending edit when navigation takes over. The toolkit
retains capture and the same hide/close cancellation rules.

For a right-side Add palette, opt in with `mpc_ui_list_drag_out` after placing
the list. Vertical swipes keep native scrolling. A deliberate leftward drag
emits BEGIN/MOVE/DROP with screen-local coordinates and the original row index.
The consumer owns only the preview and validates its canvas bounds before one
engine creation on DROP. A second contact, silent list refresh, hide or close
cancels; `mpc_ui_list_cancel_drag` handles consumer actions. Keep the callback
owner alive through screen closure. Native candidate13 observed Cardinal drag,
tap placement, scroll in both directions, outside release and canonical Undo
in `build/touch-track-1/`. Physical multi-contact cancellation remains unverified;
the ARM component fixture substitutes event delivery for its regression check.

`examples/image-diagnostic/build.sh` builds a diagnostic and a normal Global
MIDI Learn package in ignored scratch. It explicitly substitutes committed
CMD31 host `4d29e11963a97846f0531cda9ef61af678a0b4d0` for the parked CMD32 host
work, overlays the current toolkit, and uses the existing example app slot.
Its explicit build-only host patch accepts exactly four typed diagnostic
pointer receipts (0x201 DOWN, 0x202 MOVE, 0x203 UP, 0x204 CANCEL) through the
existing example intent counter and last-intent field. A consumer-header marker
enables that branch only for the diagnostic; normal examples still reject these
IDs, and unknown IDs remain failures in both builds. This extra host adaptation
allows the retained old state inode to record CANCEL before forced teardown.
The diagnostic shows an odd-width RGB image, top-left white orientation mark,
dark bottom, touch marker and event counts. Pressing its rightmost 20 pixels
hides it during the contact; Show restores it. It starts no renderer or engine.
ARM tests substitute native Image, Graphics and MouseEvent boundaries; they
do not establish native ABI execution, rendering, touch capture or teardown.
The image ABI was compared to pinned JUCE
`8ec56d15896d687711be348aaace08311ecabcc8` and exact MPC raw ELF callees/callers,
with admission ranges in `native-preferences-prepare.py`. The native image
trial observed correct RGB/orientation, drag coordinates and marker movement,
one hide-during-contact cancellation with stale release ignored, Menu/resume,
Back/reopen and balanced idle owner teardown with zero failures. Evidence is
`build/native-image-ui-qualification/native-image-1/`. Active-contact forced
owner destruction remains unobserved. Command-Main navigation during a held
contact bypassed suspension; arbitrary host navigation is not qualified.

The public image consumer also hosts a stopped-DSP Pd editor. The first native
trial exercised actual object creation, selection/movement, wiring, Unwire,
Delete, unchanged engine-written Save/fresh Reload, Menu/resume and balanced
Back (`build/pd-touch-prototype/native-touch-1/`). It exposed a consumer bug:
captured outside MOVE/UP was clamped and committed, not delivered as CANCEL.
The consumer now cancels at the canvas edge; its replacement native package
still awaits that repeated observation. The frozen Rack app's native touchscreen
trial exercised creation, Source movement, Source-to-Sink wiring, unchanged
Save/fresh Reload, Unwire, rewire, Menu/resume, balanced Back/fresh reopen and
suspended New Project owner closure (`build/cardinal-touch-prototype/native-touch-1/`).
Its old-inode readback records cleanup before the stock destructor, no retained
owner and zero failures. These prototypes use real engine
owners but fixed test-module/object palettes and DSP stopped. They establish
neither audio integration, musical performance nor a general module catalog.
No plug-in editor attachment is implied.

The launcher and public examples now demonstrate this canonical implementation
through the qualified app-list owner: native rendering, button intent, Label
refresh, Slider drag events, post-callback Back, fresh reopen and repeated
layout are observed. The earlier button-only screen balanced two cycles; the
combined Label/Slider consumer balanced one Back and then reopened at its
initial value. The public-navigation checkpoint then resumed the same suspended
value-32 consumer and captured its complete idle owner teardown through ordinary
New Project. The normal Global editor separately retained an unapplied draft
across stock Menu and tile resume, then captured complete idle owner teardown.
The same integrated source checkpoint renders the canonical Controllers panel
without the stock placeholder, with complete filled buttons, legible labels and
a visible selected marker. Native touches selected Generic MCU and restored
X-Touch Mini through two claimed and completed owner requests; the marker
repainted on the open tab, and the restored preferences file matched its exact
pre-run SHA-256. Full stock navigation, playback resource cost and audible
behavior remain under qualification.

The Global MIDI Learn assignment adapter captures MPC's existing owner at a
guarded stock startup callsite and reads target labels and current pairings on
that UI thread. The current public composition presents those values through a
native functional-group list, a native target-name list and a separate selected
assignment detail. The earlier assignment-format-1 native checkpoint exercised
the same owner/readback adapter through the superseded six-button view: it
rendered Q-Link assignments as `CH1 CC13 ABS` through `CH1 CC18 ABS`, matching
the retained XMM's type, channel, data and control fields. Four Next touches
reached targets 25-30, row selection updated target 25, and two Back/reopen
cycles ended with assignment screens and shells both created/destroyed `2/2`
and zero failures. Grouped-list-2 then rendered both native lists and exposed
failed drag scrolling, white unused viewport space and stale pixels after a
same-count group change. Grouped-list-3 observed those repairs on MPC: forward
and reverse swipes moved both lists without owner selection, later taps selected
once, Pads to Q-Links repainted, Additional Targets stayed dark, and two
released-swipe Back/reopen cycles balanced screens and shells `2/2` with zero
failures. One attempted tap immediately after reconnecting the development
viewer had no effect, while later top, scrolled and reopened first taps worked;
broad input reliability is therefore still open.

The arm-cancel-1 native checkpoint also exercised the mapping owner's queued
intent on MPC. A row touch selected target 1 once; Learn settled with the view
controller queue drained and both the owner value and UI mirror armed. A touch
on another row while armed left the original selection unchanged. Cancel then
settled both values false. A second Learn followed by Back issued the same
false intent before closing, and reopening showed an idle screen. Two screen
cycles balanced, with zero failures or timeouts, and the complete original
mapping inventory remained unchanged apart from the root-owned disposable test
copy.

Save stays hidden because no separate save-success ABI is qualified. The screen
maintains no second mapping store; MPC's existing owner persists a learned
gesture. Cancellation while MIDI delivery is queued, automatic one-gesture
behavior, busy or active-callback launcher destruction and playback cost remain
unobserved. Idle suspended launcher destruction is observed as described above.

The adapter also performs bounded visible-row freshness without adding a
mapping listener or store. One displayed pairing is read every four existing UI drains;
while armed, that bounded poll targets the fixed selected row. It compares only
the five synchronously copied owner scalars. Unchanged pairings do not decode a
label, call the formatter, allocate text or repaint. A changed pairing rebuilds
and repaints the selected detail only and publishes no Learn intent. On native
MPC, one physical Mini knob gesture changed target 24 from `CH1 CC13 ABS` to
`CH1 CC16 ABS` while the grouped screen remained armed; the visible assignment
repainted once, and an independent parse of the automatically created
disposable XMM copy found target 24 was the only changed pairing among 120.
Explicit Cancel settled the owner, mirror and pending state false. A second
Learn followed by Back settled the same false intent before close. After the
MPC process exited normally, a fresh process loaded the owner-written copy and
rendered target 24 as `CH1 CC16 ABS`, selected and idle. Original mapping and
selection hashes were subsequently restored and both disposable XMM files were
removed. Playback resource cost, audible behavior and the complete milestone
review remain open.

The full-editor native checkpoint imported the exact Akai desktop fixture into
MPC's writable user mapping directory with byte-identical source and destination
files. Copy plus manual Apply changed only target 24 from CC74 to CC75; Clear
changed only target 25 to the native unassigned values. New MIDI Mapping 9 was
saved with target 24 at CC1, and Export produced destination bytes identical to
that native common-subset source. A fresh MPC process reopened the Copy, Clear
and New results with those same semantic assignments. Importing the same desktop
file again took the visible destination-collision branch without changing the
destination, catalog count or selection. Native Export cancellation exposed a
stock empty-name error; the repaired adapter now returns from an empty chooser
result before File construction or writer entry. Native observation confirmed
that Cancel returned without an alert or destination, left the selected Copy
bytes unchanged and did not block a later byte-identical named Export. Complete
editor review remains open. The stock writer filters target IDs above 119 and does not generically
preserve all desktop metadata, so Copy and Export are not described as lossless
desktop-file operations.

Standalone mapping editing is the supported goal. Import preserves the source
file bytes, but native Copy, Save and Export may discard desktop-only targets
and metadata. Keep the original desktop file if those parts matter. Lossless
desktop editing is optional future work, not a prerequisite for using this
toolkit or the standalone editor.

Plug-in attachment is still unqualified. The no-GUI VST2 opcode probe must first
observe the real MPC editor parent and thread. A later adapter must demonstrate
the plug-in's existing parameter owner round trip and close/unload order on that
exact parent. The toolkit provides its full-button painter and native inherited
Label and Slider widget paths. Arbitrary Graphics and text entry remain
unsupported.

`mpc_ui_list_detail_rows` opts a list into readable primary/detail rows. It keeps
the existing list model, selection, scrolling and owner callbacks. Primary text
is 22px over one or two lines; secondary text is muted 18px. Both are left
aligned with 16px padding and retain native fitting without horizontal shrink.
The painter needs at least 48px (one primary line) or 74px (two); taller rows
add vertical breathing room. A null detail
callback restores the ordinary painter; existing lists do not opt in implicitly.

# Explicit 3.10 build profile

`MPCLEARN_FIRMWARE=3.10 ./ui/build.sh` selects the private 3.10 profile for the
standalone toolkit component build. The composed Native Preferences build
instead force-includes the exact command preparation's `model-build-profile.h`
in every C/C++ object. `private/mpc-profile.h` selects one profile at compile
time; the adapter still requires matching admission from its runtime owner.
This does not detect firmware at runtime or admit a binary by version name.

The 58 coordinates in `private/mpc-3.10-profile.h` are based on the exact stock
ELF SHA256 `753f838ca362d42c8cc3f5c9c3fcc604ba076b5919212e0301923a3c11c93494`.
RTTI constructors/deleting destructors establish Button/Label/ListBox/Slider
sizes `0x100/0x100/0xac/0xcc`, and the copied Button/ListBoxModel tables retain
212/68 bytes. The ListRow coordinate is already a vptr; other table coordinates
name headers. Native callbacks establish row/model fields, MouseListener slots
9/10 and hard-float magnification, 28-byte BitmapData, and the 0x48-byte copied
keyboard session. Direct branches and PC-relative literals were inspected;
numeric similarity alone is not layout evidence.

Trace locators: `/tmp/mpclearn-m25-ui-trace/report.md`,
`followup-closure.md`, `body-evidence.json`, `reviewed-branch-targets.json`,
`reviewed-literal-targets.json` and `followup-details.json` in that directory.
The external keyboard service is Engine `+0x130`, separately traced in
`/tmp/mpclearn-m25-editor-trace/report.md`; the toolkit receives that owner
through its existing adapter API. The separately recorded generic implementation
closure is `integration-closure.json` and `integration-closure.md` in the same
UI trace directory. Composed Native Preferences admission remains separate from
the standalone component gate.
Component checks substitute native widgets, callbacks, Graphics and input.
Native rendering, gestures, keyboard lifetime and playback remain device
qualification boundaries.


The production composition uses the existing entry points:

```sh
MPCLEARN_FIRMWARE=3.10 controllers/program-observer/native-preferences-build.sh \
  /path/to/exact-3.10-CE-MPC /path/to/exact-3.10-stock-MPC /data/mpclearn/dev
MPCLEARN_FIRMWARE=3.10 controllers/program-observer/native-preferences-check.sh
```

The build prepares the command/bridge package first, then verifies the exact
stock and CE inputs against `native-preferences-310-coordinates.json`. It emits
the native ranges and selected layout together. The generated command profile
and native layout directories are passed to every composed C/C++ compilation;
there is no runtime firmware fallback. `MPCLEARN_FIRMWARE=3.9.1` retains the
preservation build. Source export and final image recipe selection are separate.

The command owner installs the shared Property destructor detour first.
`native-preferences-join.py` checks its displaced pair and literal relocation,
then checks the native UI's untouched 320-byte tail. Static preparation still
verifies the complete destructor. Existing prepare scripts and runtime
installers retain admission ownership; the coordinate manifest is traced data.
Inherited prefix/fold guards remain identified in provenance; separately traced
complete bodies cover the ListBox/Slider and text-session tails. The text-close
facade plus active setter is 0x1f8 bytes, including both final literals.

The adapter retains CE's 120 Global MIDI Learn rows. In the 3.10 profile it maps
CE IDs 118/119 to native 119/120 at label/Target construction boundaries; native
118 (Browser Help) is not a CE row. Native 95 supplies Akai's Track Mixer label.
Selection stays with the existing screen instance, and the native owner still
performs lookup, Learn, Clear and manual commit. This does not rewrite `.xmm`
files or establish compatibility of old serialized numeric target IDs.

The ARM component calls the actual resolver at nonzero bias before substituting
native callbacks. Independent expected addresses and distinct old/new stack
slots catch stale function, owner and Preferences frame coordinates. The normal
and public-example modes exercise Controllers/Detect and Global Learn or the
existing toolkit example. Native widget execution, filesystem/modal interaction,
rendering, hardware input and playback are still substituted or unobserved;
passes here do not establish native fit or audio compatibility. Parked plugin
controls remain disabled in the production composition.
