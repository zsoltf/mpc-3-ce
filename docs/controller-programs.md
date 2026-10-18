# Controller programs and modes

Choose the matching CE profile in MPC Preferences > CONTROLLERS, select the
exact From Controller and To Controller ports, and save it. Akai profiles use
an explicit saved program on the controller. They do not recognize factory
presets, upload programs, change modes, or send LED feedback. Use a copy of your
musical program and select it on the controller before enabling its CE row.

Keep keyboard/pad notes and sequencer clock routed normally. Do not assign the
same CE control messages in MPC Global MIDI Learn: that would apply two actions.
An absolute knob/fader takes control when it reaches or crosses the MPC value.
Changing layer, bank, project, or connection requires pickup again.

The [offline program tools](../controllers/program-tools/README.md) can patch
supported original program files to these assignments and produce an exact
change report. They do not send programs to a controller. Keep the original
file; the tool guide identifies each output format and its loading method.

## Launchpad X and Launchpad Pro Mk3

Choose LAUNCHPAD X PROGRAMMER or LAUNCHPAD PRO MK3 PROG in CONTROLLERS.
Choose the ordinary MIDI input/output pair, not the DAW pair: LPX MIDI on X,
LPProMK3 MIDI on Pro. Windows orders X's ordinary pair second and Pro's first;
MPC names must be selected from its actual port list.

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
Programmer is active. LPX Legacy mode is not covered by the documented readback.

Press **Session** for MPC's Clip Matrix. The grid shows eight tracks across
and eight rows down. The first page shows Arrangement, then scenes 1-7; the
next page starts at scene 8. Left/Right pages tracks; Up/Down pages rows. Press
a slot to launch it, or a right-side button to launch that whole row, using
MPC's launch quantization. Arrangement uses MPC's current Sequence.

Hold **Capture MIDI** on X, or **Projects** on Pro, and press a slot to stop
its track. Hold that modifier and press a scene button to stop tracks currently
playing or queued in that scene, including tracks outside the visible eight.
MPC stops per track: this also cancels another queued transition on those
participating tracks.

To record an empty slot, arm its track, then hold **Session** and press the
slot. Hold Session and press the recording slot again to finish. MPC retains
its quantization, fixed-length and Record To settings; finishing may continue
playing or overdubbing according to those settings. A pending recording action
is not a request to toggle it again. Arrangement is not a recordable clip slot.

Clip colors follow MPC: dim white is empty, purple is occupied/stopped, green
is playing, red is recording, and amber is overdubbing or stopping. Flashing
indicates a queued change; recording and overdubbing pulse. Unavailable slots
stay dark. Scene lights use the whole native scene, including offscreen tracks.

Hold Capture MIDI/Projects and press Session to enter the **mixer**. On X,
Left/Right (CC93/94) bank eight tracks. Its right column selects Volume, Pan,
Send 1, Q-Link, mixer grid, Play, Stop and Record. In Q-Link, Up/Down select
Q-Links 1-8 / 9-16.

On Pro, the bottom Volume, Pan, Sends and Device buttons select those layers;
Arm, Mute and Solo return to the mixer grid. Left/Right bank mixer tracks.
The left Play and Record buttons control transport, and bottom Stop Clip stops
MPC. In Q-Link, the left Up/Down buttons select Q-Links 1-8 / 9-16.

For drum steps, select a Drum track in MPC. X Note enters a 64-step view for
one sound; X Custom enters a 16-step view for four sounds. Pro Sequencer enters
the 64-step view and binds the currently selected Drum track and clip on every
press; its right-side Steps button switches between the two views.
Session returns to clips. Entering a drum view binds that Launchpad to the
selected native drum track and clip. It keeps that target, sound and time page
while the other surface launches clips or MPC selects another track. Using
Pro Steps or X Note/Custom to switch between the two drum views keeps the binding. Deleting or replacing its
track/clip clears it.

To change the MPC track or clip being sequenced:

1. Select the new Drum track and clip on the MPC.
2. On Pro, press Sequencer. This resets the sound and time page to the selected
   native target. On X, press Session, then Note or Custom.

Up/Down changes drum sounds within the bound track; it does not select another
MPC track. Pro Steps keeps the existing target, sound and time page. The XL3
Pad mixer follows the selected Drum program separately.

Read the step grid from top left to bottom right. The single-sound view uses
all eight rows for steps 1-64. The four-sound view uses two rows per sound,
16 steps each. Up/Down moves the sound, or four-sound group; Left/Right moves
by the displayed number of steps. Step duration follows MPC's time-correction
setting. Press a pad to add or remove a native MPC note; MPC Undo/Redo applies.
Lit pads show native steps in a stable CE colour for each drum slot, shared
between X/Pro and both views. The palette repeats every 16 pads; it does not
match MPC custom pad colours. The separate moving highlight follows playback.
Inactive steps stay dark except at the playhead; unavailable cells always stay
dark. The MPC screen can remain on Main while editing.

In Volume, Pan, Send and Q-Link layers, each grid column controls one parameter;
the eight pads from bottom to top set levels from 0 to 127 in seven equal
steps. The mixer grid uses the bottom four rows for Select, Mute, Solo and Arm,
and the top four rows for volume 0%, 33%, 67% and 100%. Available values and
mixer states appear on the grid; unavailable controls stay dark. These are CE
grid layers, not the hardware DAW fader engine. Buttons and grid positions use
the official Programmer addresses: grid Note11-88, right CC89 down to19.

## Launch Control XL 3

Choose LAUNCH CONTROL XL 3 DAW and its dedicated DAW input/output pair in
CONTROLLERS. Turn Track, Control and Global input off for this DAW port in MIDI/Sync;
leave its Custom/musical ports unchanged. CE enters DAW Mixer mode. To change modes, press Mode, choose
the button labelled DAW Mixer or DAW Control, then press Mode again.

| Control | DAW Mixer | DAW Control |
| --- | --- | --- |
| Eight faders | Volume of the current eight tracks | Volume of the current eight tracks |
| Top encoder row | Send 1 | Selected Q-Link or effect parameter page |
| Middle encoder row | Send 2 | Unassigned |
| Bottom encoder row | Pan | Unassigned |
| Track arrows | Previous/next eight tracks | Previous/next eight tracks |
| Page arrows | Unassigned | Previous/next control page |

In DAW Mixer, hold **Shift** and press **Solo/Arm** to switch between Track
and Pad mixer. Pad mixer follows the selected Drum program: faders change pad
volume, the bottom encoders change pan, and the two button rows are Solo and
Mute. Track arrows bank through all 128 pads. Fader labels show `Pad A01` through
`Pad H16`, and Solo/Arm changes colour in Pad mixer. Unsupported pad sends stay
dark with `-` values; pad arm and selection are unavailable. The same chord
returns to Track mixer and its existing Solo/Arm and Mute/Select choices.
Control pages remain available, with faders retaining the chosen mixer target.

Control pages cycle through Q-Links 1-8, Q-Links 9-16, then effect slots 1-4.
Each effect page exposes its first eight available parameters in the current
MPC context. Q-Link pages follow MPC's active Q-Link mode; select the desired
mode and assignments on MPC first. The middle and bottom encoder rows remain
dark in Control mode.

Solo/Arm switches the upper button row between those actions. Mute/Select
switches the lower row. The buttons' lights follow MPC state. Play and Record
control MPC transport. Faders use pickup; encoders act relatively.

The display shows control names and values on a 0-127 scale. Hold Shift and
turn a control to preview it. An unavailable encoder value shows `-`.
Disable the row before unplugging to return the controller to Standalone;
unplugging first can prevent that exit.

## MPK mini mk3 and LPD8 Mk2

| Setting | MPK mini mk3 | LPD8 Mk2 |
| --- | --- | --- |
| Profile | MPK MINI MK3 CE | LPD8 MK2 CE |
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
