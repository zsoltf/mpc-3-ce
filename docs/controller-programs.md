# Controller programs and modes

## Connect a controller

1. Set the device mode listed below, then connect it by USB.
2. In Preferences > MIDI/Sync, set its input as listed below.
3. Open Preferences > CONTROLLERS and press **Detect**. Detect adds
   every connected controller marked "Detect" that is not listed yet, and says
   what it added. It never changes or removes existing rows.
4. For other controllers press **Add**, choose the profile, the From and To
   ports, then **Apply**.

To switch, select a row: **Edit** changes its profile or ports, **Disable**
pauses it, **Remove** deletes it. A new install starts with the full X-Touch.
The page lists up to 64 external MIDI ports; each input or output port of every
connected MIDI device counts. With more connected, unplug a device to add or
edit controllers.

| Controller | Profile | From / To port | Device mode | MIDI/Sync input for that port |
| --- | --- | --- | --- | --- |
| X-Touch (Detect) | X-Touch | `X-TOUCH_INT` | MC over USB | Track, Global, Control off |
| X-Touch Mini (Detect) | X-Touch Mini | `X-TOUCH MINI MIDI 1` | MC (hold MC while connecting) | Track, Global, Control off |
| Launchpad X (Detect) | Launchpad X | `LPX MIDI In` | Programmer, set by CE | Track, Global, Control off |
| Launchpad Pro Mk3 (Detect) | Launchpad Pro Mk3 | `LPProMK3 MIDI` | Programmer, set by CE | Track, Global, Control off |
| Launch Control XL 3 (Detect) | Launch Control XL 3 | `LCXL3 1 DAW In` | DAW, set by CE | Track, Global, Control off |
| MPK mini mk3, LPD8 Mk2 | MPK Mini Mk3, LPD8 Mk2 | its USB port | CE program loaded | Unchanged |
| MPK249 | MPK249 | `Port A` (or `Port B`) | CE program, transport MMC | Receive MMC off |
| Other MCU or HUI surface | Generic MCU, Generic HUI | its MCU/HUI port | MCU or HUI | Track, Global, Control off |

Akai profiles use an explicit saved program on the controller. They do not
recognize factory presets, upload programs, change modes, or send LED feedback.
Use a copy of your musical program and select it on the controller before
enabling its CE row.

Keep keyboard/pad notes and sequencer clock routed normally. Do not assign the
same CE control messages in MPC Global MIDI Learn: that would apply two actions.
An absolute knob/fader takes control when it reaches or crosses the MPC value.
Changing layer, bank, project, or connection requires pickup again.

## Sampler controls on MPC 3.10

Choose the input, monitoring and threshold on MPC Sampler once, then return to
Main or another page. **Arm** waits for the configured threshold; **Start**
begins an armed recording immediately; **Stop** finishes a recording or cancels
an armed sampler. Finish the native Keep/Discard dialog on the MPC before
arming again. Leaving the Sampler page still invokes its normal stop.
Check MPC's Browser > Audition level when comparing a sample preview with
project playback.

### MPC Live II

Hold **Shift**, then tap **Erase** to start sampling immediately from the
current screen. Use **Shift + Erase** again to stop and open native
Keep/Discard. There is no separate arm press. A brief native banner confirms
the sampler state.

Ordinary Erase keeps its context-dependent function, including Erase + pad.
Pressing Erase before Shift keeps that gesture with native Erase. Record,
Play and Stop keep their existing functions. This shortcut is for MPC Live II
on MPC 3.10; other models retain their native Shift + Erase behavior.

### External controllers

| Controller | Hold | Arm | Start | Stop/cancel |
| --- | --- | --- | --- | --- |
| Full X-Touch | Shift | Record | Play | Stop |
| X-Touch Mini | Layer A + Layer B | Bottom Record | Bottom Play | Bottom Stop |
| Launchpad Pro Mk3 | Shift | Left Record | Left Play | Bottom Stop Clip |
| Launchpad X, mixer pages only | Capture MIDI | Record Arm | Mute (ordinary Play) | Solo (ordinary Stop) |
| Launch Control XL 3 | Shift | Record while idle | Record while armed, or Play | Record while recording |

On Launchpad X, tap **Custom** to open a mixer page first. Its Session and Step
Capture gestures keep their existing functions. On Pro Mk3, Shift + bottom
Record Arm/Mute still performs Undo/Redo.

While the modifier is held, the three-button mappings show Stop at idle,
Record while armed, and Record + Play while recording. XL3 shows Record white
at idle, red while armed, and pulsing red with Play green while recording.
XL3's Record action follows the native state: a threshold crossing can start
recording before the next press, making that press Stop. Finalizing or
unavailable clears these lights; XL3 Record then does nothing.

Release the modifier for ordinary sequencer transport. Sequencer Record keeps
its existing function. These shortcuts use the listed CE controller profiles;
they do not add Global MIDI Learn targets. The MPC Live II shortcut above
uses its own physical buttons and does not require a controller profile.

## Drum-pad MIDI mute on MPC 3.10

Mute a Drum pad on the MPC or through a CE pad mixer. Its native effective mute
also controls MIDI notes sent from that pad: new notes are blocked while muted,
and notes already sent are ended. Unmuting does not replay those notes. Audio
mute, pad solo and controller feedback continue to follow the MPC's native
state; there is no separate MIDI mute switch.

This applies to output that follows the Drum pad in the routing chain. MIDI
sent before a later Drum mapping is not owned by that pad. Use distinct MIDI
channels or pitches when independently muting instruments that share an
external destination: MIDI 1.0 cannot send a source-specific Note Off for two
simultaneous notes on the same destination, channel and pitch.

## X-Touch Mini

The [Mini visual guide](https://mpc3ce.com/mini-guide.html) shows the layout,
including both held menus and printable reference strips.
Use MC mode and the X-Touch Mini profile.
Reconnect starts with **Volume** knobs and **Select** track buttons.

The top row always operates the eight tracks in the current track bank,
including while the knobs are on Sends or Inserts. **Select** lights the
selected track, **Mute** lights muted tracks, **Solo** lights soloed tracks and
**Record** lights armed tracks. Record toggles each track's record arm; the
bottom-row Record button controls transport recording.
Missing or unsupported targets stay dark.

Tap **Layer A** to cycle Volume, Pan, Q-Links 1-8, Q-Links 9-16, Sends and
Inserts. Tap **Layer B** to cycle Select, Mute, Solo and Record, then back to Select. A bare tap changes mode
on release. Hold a Layer button to show its top-row menu:

| Top button, left to right | Hold A: knobs and actions | Hold B: track buttons |
| --- | --- | --- |
| 1 | Volume | Select |
| 2 | Pan | Mute |
| 3 | Q-Links 1-8 | Solo |
| 4 | Q-Links 9-16 | Record |
| 5 | Sends | No action |
| 6 | Inserts: return to slots | No action |
| 7 | Save | No action |
| 8 | Add Track | No action |

Only the current mode is lit in a held menu. Release the Layer button to restore
track lights; a menu choice does not also cycle. If both are held, A takes menu
precedence and neither release cycles.

The bottom row is **Undo, Redo, Rewind, Forward, Loop, Stop, Play, Record**.
Rewind/Forward move by beats and repeat while held. Stop again while stopped
returns to the start. Hold **B + Rewind/Forward** for the previous/next track
bank, in every knob mode. Hold **A + Rewind/Forward** for the previous/next native
MPC Q-Link mode while on Q-Links, or parameter page inside an insert. It has no
action in other modes. A modified navigation gesture does not become transport
if the Layer button is released first. With both Layers held, bottom Record/Play/Stop control the sampler; other
bottom buttons and navigation are inactive. The top row keeps the A menu.

Volume/Pan knob pushes select their tracks without resetting levels. Q-Link
pushes do nothing. In Sends, knobs 1-4 edit the selected track's sends and 5-8
edit the matching return volumes; pushes do nothing. In Inserts, push 1-4 at the
slot list to open an occupied slot or its native chooser. Turn to edit the open
slot's parameters; push toggles native two-state parameters. Select Inserts
again to return to slots. Finish native choosers on the MPC.

Rings follow MPC values, including touchscreen changes; unavailable targets
clear. The master fader always controls master output. Save uses MPC's normal
naming dialog when needed. Normal-MIDI presets are not rewritten.

## Launchpad X and Launchpad Pro Mk3

Detect, or Add with the Launchpad X or Launchpad Pro Mk3 profile, uses the
ordinary MIDI pair, not the DAW pair: LPX MIDI on X, LPProMK3 MIDI on Pro.

In MPC MIDI/Sync, turn **Track, Control and Global off for that selected
Launchpad input**. CE subscribes to the port independently. Track input can
play or record grid notes; Control input can also invoke legacy MIDI Learn
actions, including Undo/Redo on the first two top-row pads. Keep other ports
and clock routes unchanged. CE does not change these MPC routing settings.
This applies to the Programmer port, not a keyboard's shared musical port.

CE queries the prior mode/layout before entering Programmer. An unknown reply
leaves the row inactive without mode changes. Disable the row before unplugging
to restore the captured mode/layout, including a previous Programmer layout.
The separate DAW-enable flag is never changed. Unplugging can prevent restoration.
On Pro, Note, Chord and standalone sequencer controls are unavailable while
Programmer is active. Pro's Fixed Length button is unmapped; recording uses
MPC's configured fixed length. LPX Legacy mode is not covered by the documented
readback.

Both Launchpads have the same three views. **Session** shows MPC's clips, the
step-view button (**Note** on X, **Sequencer** on Pro) shows the drum step
sequencer, and **Custom** shows the mixer pages: Volume, Pan, Sends and Device
(Q-Links), and on X also the mixer grid. Custom reopens the page used last.
Only buttons that do something in the current view are lit.

Press **Session** for MPC's Clip Matrix. The grid shows eight tracks across
and eight rows down. The first page shows Arrangement, then scenes 1-7; the
next page starts at scene 8. Left/Right pages tracks; Up/Down pages rows. Press
a slot to launch it, or a right-side button to launch that whole row, using
MPC's launch quantization. Arrangement uses MPC's current Sequence. Pressing a
clip also selects it in MPC, so a step view opened next shows that clip.
Pressing a playing clip follows its MPC Launch Mode (Clip Settings): Trigger
restarts it, Toggle stops it.

Hold **Capture MIDI** on X, or **Projects** or **Stop Clip** on Pro, and press
a slot to stop its track. Hold that modifier and press a scene button to stop tracks currently
playing or queued in that scene, including tracks outside the visible eight.
MPC stops per track: this also cancels another queued transition on those
participating tracks.

To record an empty slot, arm its track, then hold **Session** and press the
slot. Hold Session and press the recording slot again to finish. MPC retains
its quantization, fixed-length and Record To settings; finishing may continue
playing or overdubbing according to those settings. A pending recording action
is not a request to toggle it again. Arrangement is not a recordable clip slot.

On MPC 3.10, hold **Duplicate** on Pro or **Custom** on X, press a source
clip, then an empty destination. The source flashes; keep holding for repeated
pastes and use arrows to page. Use right-side scene buttons for whole rows:
press the source row, then the destination row. Row paste uses MPC's own Copy
Row action and can replace an existing row, including tracks offscreen.
Release ends the source selection. Copying does not launch clips or change
views; an unused X Custom tap opens the mixer on release. Arrangement is
outside these editing gestures.

Delete a clip with **Clear + pad** on Pro or **Custom + Capture MIDI + pad**
on X. Use the same modifier with a scene button to delete that whole row;
following rows move up. Undo/Redo are **Shift + Record Arm/Mute** on Pro and
**Capture MIDI + Left/Right** on X. With X Custom held, arrows always page, even with Capture
held. MPC owns clip contents, history and recording eligibility.


### Direct editing

In the Session clip grid, hold **Shift + Duplicate** on Pro or **Session +
Custom** on X. Press a clip to **double its active length with its events**,
or a right-side scene button to **Insert & Capture** before that row.
Insert & Capture copies MPC's currently playing clips into the new row; it
does not start the new row. MPC owns each action's limits and Undo/Redo.

On Pro, hold **Quantise** and press a clip to apply MPC's current Timing
Correct Type, Events scope (All/Selected/Range), timing and groove settings.
Timing Correct must be on; when it is off, events stay unchanged. For audio
clips, use Start; End, Length and Legato apply to MIDI notes only. Launchpad X
has no direct Quantize shortcut.

Copy makes another clip or row. Double with events repeats notes or audio
regions into twice the active length; with looping enabled, MPC extends the
loop window rather than necessarily moving the stored End marker.

**Example:** Copy a clip to an empty slot. Hold Shift + Duplicate (Pro) or
Session + Custom (X), then press the new clip. Its events repeat into the
longer active region; the original is unchanged.

The [illustrated Launchpad guide](https://mpc3ce.com/launchpad-guide.html)
includes button maps and a printable reference. The X-Touch's dedicated
**Save** key saves the project; an unnamed project opens MPC's normal save
dialog.

A short white flash on the target acknowledges that MPC accepted an edit.
Normal clip lights return after about half a second. MPC settings can leave
contents unchanged even when an action is accepted.

Clip lights, including the Arrangement row, follow the MPC clip launcher: empty
slots are off, a clip with content shows its MPC track colour, and the playing
clip pulses white. A queued launch flashes white against the track colour; a
queued stop flashes white. Recording pulses red and overdubbing pulses amber;
their queued changes flash. Unavailable slots stay dark. Scene buttons show blue
for ready and green for playing; empty scenes are off. Scene lights use the whole native scene, including offscreen tracks.

On X, press **Custom** for the mixer pages. Left/Right (CC93/94) bank eight
tracks. The right column selects Volume, Pan, Send 1, Device (Q-Links) and the
mixer grid, lit green for the current page, then Play, Stop and Record. The
mixer grid's bottom four rows are Select, Mute, Solo and Arm for eight tracks,
and its top four rows are volume. In Device, Up/Down select Q-Links 1-8 / 9-16.

On Pro, press **Custom**, or a bottom Volume, Pan, Sends or Device button, for
the mixer pages; the current page is lit green. Left/Right bank mixer tracks.
The row of eight Track Select buttons under the grid acts on the eight tracks
shown: the clip columns in the clip grid, or the mixer bank. On its own it
selects a track (the selected track is white). **Record Arm**, **Mute**,
**Solo** and, in the clip grid, **Stop Clip** turn it into arm, mute, solo or
stop buttons for those tracks: tap the mode button to keep it, tap it again to
return to Select, or hold it for a moment. Armed tracks show red, muted amber,
soloed blue and tracks with a playing clip green. The grid stays as it is.
The left Play button starts playback, pauses it in place when already playing,
and resumes on the next press. Record controls recording. Stop Clip never stops MPC:
in the clip grid hold it and press a slot or scene; in a drum view press it to
stop the track of the clip open there. Stop MPC with Play or on the MPC. In
Device, the left Up/Down buttons select Q-Links 1-8 / 9-16.

For drum steps, select a Drum track in MPC. The step-view button is Note on X
and Sequencer on Pro. It opens a drum view: 64 steps for one sound,
or 16 steps for four sounds. Press it again in a drum view to switch between
the two; Pro's right-side Steps button switches them too. A Launchpad reopens
the view it used last. Session returns to clips. Entering a drum view binds that
Launchpad to the selected native drum track and clip. It keeps that target,
sound and time page while the other surface launches clips or MPC selects
another track. Switching between the two drum views keeps the binding.
Deleting or replacing its track/clip clears it.

To program a clip without launching it, hold the step-view button and press the
clip in the clip grid. The MPC selects that clip and this Launchpad opens its
drum view on it. Picking an empty slot creates a new empty clip there, at MPC's
new-clip length, and opens it; playback continues. To program it from the
other Launchpad, press that one's step-view button. In the clip grid these
buttons open their view when released.

To change the MPC track or clip being sequenced:

1. Select the new Drum track and clip on the MPC, or pick it as above.
2. Press Session, then the step-view button. This resets the sound and time
   page to the selected native target.

Up/Down changes drum sounds within the bound track; it does not select another
MPC track. The XL3 Pad mixer follows the selected Drum program separately.

Read the step grid from top left to bottom right. The single-sound view uses
all eight rows for steps 1-64. The four-sound view uses two rows per sound,
16 steps each. Up/Down moves the sound, or four-sound group; Left/Right moves
by the displayed number of steps. Step duration follows MPC's time-correction
setting. Press a pad to add or remove a native MPC note; MPC Undo/Redo applies.
Step colours identify the drum sound: each drum slot has its own fixed CE
colour, shared between X/Pro and both views, so the four-sound view shows four
colours. The palette repeats every 16 pads; it does not match MPC custom pad
colours. The separate moving highlight follows playback.
Inactive steps stay dark except at the playhead; unavailable cells always stay
dark. The MPC screen can remain on Main while editing.

In Volume, Send and Q-Link layers, each grid column controls one parameter;
the eight pads from bottom to top set levels from 0 to 127 in seven equal
steps. Pan is centred: the two middle pads set exact centre, the pads below
pan left and the pads above pan right, and the column lights outward from the
centre (white at exact centre). The X mixer grid uses the bottom four rows for Select, Mute, Solo and Arm,
and the top four rows for volume 0%, 33%, 67% and 100%. Available values and
mixer states appear on the grid; unavailable controls stay dark. These are CE
grid layers, not the hardware DAW fader engine. Buttons and grid positions use
the official Programmer addresses: grid Note11-88, right CC89 down to19.

## Launch Control XL 3

[Launch Control XL3 guide](https://mpc3ce.com/xl3-guide.html): setup, Q-Links,
effects, Track and Pad mixing, and display feedback.

Detect, or Add with the Launch Control XL 3 profile, uses its DAW pair. Turn
Track, Control and Global input off for this DAW port in MIDI/Sync; leave its
Custom/musical ports unchanged. CE enters DAW Mixer mode. To change modes, press Mode, choose
the button labelled DAW Mixer or DAW Control, then press Mode again.

| Control | DAW Mixer | DAW Control |
| --- | --- | --- |
| Eight faders | Volume of the current eight tracks | Volume of the current eight tracks |
| Top encoder row | Send 1 | Selected Q-Link or effect parameter page |
| Middle encoder row | Send 2 | Unassigned |
| Bottom encoder row | Pan | Unassigned |
| Track arrows | Previous/next eight tracks | Previous/next eight tracks |
| Page arrows | Unassigned | Previous/next control page |

In DAW Mixer, hold **Shift** and press a track's lower-row button to select
that MPC track. A Drum track opens the Pad mixer on its pads; any other track
returns to the Track mixer. While Shift is held, the lower row shows the
current eight tracks, with the selected track green. Hold **Shift** and press
**Solo/Arm** to switch between Track and Pad mixer. Pad mixer follows the selected Drum program: faders change pad
volume, the bottom encoders change pan, and the two button rows are Solo and
Mute. Track arrows bank through all 128 pads. Fader labels show `Pad A01` through
`Pad H16`, and Solo/Arm changes colour in Pad mixer. Unsupported pad sends stay
dark with `-` values; pad arm and selection are unavailable. The same chord
returns to Track mixer and its existing Solo/Arm and Mute/Select choices.
Control pages remain available, with faders retaining the chosen mixer target.

### Q-Links and effects

The XL3 has built-in Q-Link control and display feedback through CE's DAW
profile; no Global MIDI Learn assignment is needed.

1. Press **Mode**, choose **DAW Control**, then press **Mode** again.
2. Use the **Page arrows** to select Q-Links 1-8 or Q-Links 9-16.
3. Turn the **top row of eight encoders** to adjust the selected Q-Links.
4. Select the desired Q-Link mode and assignments on the MPC. The XL3 follows
   the active MPC Q-Link mode.

Control pages cycle through Q-Links 1-8, Q-Links 9-16, then effect slots 1-4.
Each effect page exposes its first eight available parameters in the current
MPC context. The middle and bottom encoder rows remain dark in Control mode.
The display shows Q-Link numbers or available effect parameter names and their
current values on a 0-127 scale, including changes made on the MPC.
The XL3's knob LEDs indicate availability; they are not value rings.

### Buttons and feedback

Solo/Arm switches the upper button row between those actions. Mute/Select
switches the lower row. The buttons' lights follow MPC state. Play starts, pauses
in place, and resumes playback; Record controls recording. Faders use pickup;
encoders act relatively. In Track mixer, each column's knob lights show that
track's MPC colour; Pad mixer and Control pages keep their fixed colours.

The display shows control names and values on a 0-127 scale. Hold Shift and
turn a control to preview it. An unavailable encoder value shows `-`.
Disable the row before unplugging to return the controller to Standalone;
unplugging first can prevent that exit.

## MPK mini mk3 and LPD8 Mk2

| Setting | MPK mini mk3 | LPD8 Mk2 |
| --- | --- | --- |
| Profile | MPK Mini Mk3 | LPD8 Mk2 |
| Eight knobs | Absolute CC70-77, min 0, max 127 | CC70-77, min 0, max 127 |
| Knob channel | Keybed / Controls channel 1 | Knobs channel 16 |
| Optional eight pad CCs | Bank A CC40-47 | CC40-47 |
| Optional pad channel/type | Pad channel 10; CC mode, subject to release requirement below | Pad channel 10, Momentary |

Prepare the program in the matching official Akai editor on a computer:

1. Connect the controller by USB and read the program you want to adapt:
   **R (Receive)** on Mini, or **Get** on LPD8 Mk2. Save the original to your
   computer with **File > Save Program** or **File > Save**, respectively.
   The current editors use `.mpkmini3` and `.lpd8mk2` files.
2. Change the knob assignments to the table above. Add pad CC assignments only
   if you want the optional actions below, preserving their note assignments.
   Save this edited copy under a different filename.
3. To try the copy temporarily, use Mini's **File > Send Program > RAM** or
   LPD8 Mk2's **Send to RAM**. To keep it in the controller, send it to a
   chosen program slot with **S (Send)** or **Send**. This overwrites that
   slot. Select that program when using CE.

Mini knobs share their MIDI channel with the keyboard and joystick. Both
controllers' pad channel is shared by Note, CC and Program Change modes; the
LPD8's pad Type also applies to notes. The optional layout therefore requires
these settings already to fit your musical routing. Preserve note assignments,
pressure and full-level settings. Leave pads in Note mode to play; select the
physical CC mode only when you want the optional actions below. Only CC
messages become CE commands. Program Change and notes do not.

| Pad / CC | Action |
| --- | --- |
| 1 / 40 | Previous eight tracks |
| 2 / 41 | Next eight tracks |
| 3 / 42 | Knobs: Volume (default after reconnect) |
| 4 / 43 | Knobs: Pan |
| 5 / 44 | Knobs: Send 1 |
| 6 / 45 | Knobs: Send 2 |
| 7 / 46 | Knobs: Q-Links 1-8 |
| 8 / 47 | Knobs: Q-Links 9-16 |

The decoder requires a nonzero CC press and zero release, and ignores duplicate
held presses. The Mini editor guide does not document a Momentary setting or
the release value in CC mode. Confirm a zero release from the selected program
before using its optional pad actions repeatedly; the knobs do not depend on
this check. LPD8 Mk2 documents Momentary as a pad Type. No note number or LED
color is assumed by CE.

## MPK249

Set the transport buttons to Type **MMC** in Edit mode. In MPC Preferences >
MIDI/Sync, turn **Receive MMC off** so CE handles each message once. Leave MIDI
Clock Receive/Send and musical Track routing unchanged.
**MMC/MIDI Clock** also sends the MMC commands CE uses, plus MIDI transport
messages; the controller may display this as **MMC/MIDI**.

Assign faders, knobs and switches to USB A, using A1/A2/A3 for hardware banks
A/B/C, and select the MPK249 Port A input/output pair in CONTROLLERS.
Keep keys, pads, wheels and
pedals on their existing assignments. Set faders and knobs to absolute CC,
range 0-127; switches to normal CC, Momentary, On 127 / Off 0.

| Hardware bank / MIDI channel | Faders CC16-23 | Knobs CC24-31 | Switches CC40-47 |
| --- | --- | --- | --- |
| A / 1 | Volume 1-8 | Pan 1-8 | Mute 1-8 |
| B / 2 | Send 1, tracks 1-8 | Send 2, tracks 1-8 | Solo 1-8 |
| C / 3 | Q-Links 1-8 | Q-Links 9-16 | Arm 1-8 |

USB B is also supported for assignable controls: use B1/B2/B3 and select Port B.
These assignments do not establish the transport's MMC USB destination;
Akai's guide does not specify it. CE handles MMC on the selected input.
If transport arrives on a separate port, add a second MPK249 row with that
port's input/output pair. Both input and output selections must be distinct
between rows. Do not select Remote solely because of its name.

This profile controls the first eight playable tracks. Transport accepts MMC
Stop, Play, Deferred Play and Record Strobe.
Fast Forward, Rewind and other MMC commands have no CE assignment.

## Setup references

Assignments above are the CE setup contract, not manufacturer defaults.

- [X-Touch Mini Quick Start Guide](https://cdn-media.empowertribe.com/a2d3dedbd371482db5c9906c06d301f4/QSG_BE_0808-AAF_X-TOUCH-MINI_WW.pdf),
  pages 18-19: MC assignments, Layer A (Marker) and Layer B (Nudge).
- MPK mini III Program Editor guide v1.0, pages 7-13, included in the
  [official editor 1.0.4](https://cdn.inmusicbrands.com/akai/MACEDITORS/AkaiMPKMiniIIIProgramEditorOSX%201.0.4.dmg).
- LPD8 Mk2 Program Editor guide v1.1, pages 4-8, included in the
  [official editor 1.0.2](https://cdn.inmusicbrands.com/akai/MACEDITORS/AkaiProfessionalLPD8mk2EditorOSX%201.0.2.dmg).
- [MPK249 guide v1.0](https://cdn.inmusicbrands.com/akai/attachments/MPK249/MPK249%20-%20User%20Guide%20-%20v1.0.pdf),
  pages 14-16, 19 and 39; [MPK2 MMC setup](https://support.akaipro.com/en/support/solutions/articles/69000800909-akai-pro-mpk2-series-setup-with-mpc-2-0-software).
- [MIDI Machine Control RP-013 v1.0](https://midi.org/midi-machine-control),
  simple command format and Stop/Play/Deferred Play/Record Strobe.
- [MPC MIDI port settings](https://support.akaipro.com/en/support/solutions/articles/69000804431-akai-pro-mpc-series-configuring-midi-ports-for-multi-midi-control),
  Receive MMC and clock settings.
- [Launch Control XL 3 DAW modes](https://userguides.novationmusic.com/hc/en-gb/articles/26190532073234-Controlling-your-DAW-with-the-Launch-Control-XL-3)
  and [programmer's DAW mode](https://userguides.novationmusic.com/hc/en-gb/articles/27840466544402-Launch-Control-XL-3-programmer-s-DAW-mode).

Launchpad protocol sources:
[LPX MIDI](https://userguides.novationmusic.com/hc/en-gb/articles/24001489325330-MIDI-on-Launchpad-X),
[LPX surface](https://userguides.novationmusic.com/hc/en-gb/articles/24001475492498-Controlling-the-Launchpad-X-surface),
[Pro Mk3 programmer reference](https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/LPP3_prog_ref_guide_200415.pdf),
pages 6-8 and 18-21.
