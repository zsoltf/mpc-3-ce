/* Illustrations teach gestures only. They never send MIDI or model MPC state.
 * Mapping sources: docs/xtouch-cheatsheet.md, docs/controller-programs.md,
 * mirror-input-core.h and novation-runtime.inc. XL3 geometry follows the
 * Novation-derived 628 x 602 drawing already used by index.html. X-Touch
 * positions follow the current manufacturer top view, product 0808-AAD. */
(() => {
  'use strict';
  const root = document.querySelector('[data-controller-guide]');
  if (!root) return;
  const device = root.dataset.controllerGuide;
  const NS = 'http://www.w3.org/2000/svg';
  const palette = { blue:['#8bc5ff','#25476a'], green:['#80dfa4','#235239'], yellow:['#f1da70','#5e5124'], red:['#ff9696','#642e35'], pink:['#f59ed2','#643252'] };
  const action = (ids, verb, label, color='blue') => ({ids:ids.split(' '),verb,label,color});
  const press = (ids,label,color) => action(ids,'press',label,color);
  const turn = (ids,label) => action(ids,'turn',label);
  const move = (ids,label) => action(ids,'move',label);
  const hold = (ids,label) => action(ids,'hold',label,'pink');
  const step = (title,instruction,result,actions) => ({title,instruction,result,actions});
  const flow = (title,reference,steps) => ({title,reference,steps});
  const xt = [
    flow('Mix tracks','#strips',[
      step('Choose Track','Press Track to return the eight strips to the track mixer.','The scribble strips identify the current eight tracks. Each column controls the track above it.',[press('track','Track')]),
      step('Set volume and pan','Move a track fader for volume. Turn the encoder above it for pan; push the encoder to centre pan.','The example highlights strip 1. The same controls repeat across all eight strips. Master adjusts the master output.',[move('f1 master','Track / master fader'),turn('k1','Track encoder')]),
      step('Select, mute, solo, arm','Press Select to select a track, Mute to mute it, Solo to hear it alone, or Rec to change its record-arm state.','Record arm is available only on tracks that support it. These colours explain the actions; they are not a simulation of the lights.',[press('select1','Select','blue'),press('mute1','Mute','green'),press('solo1','Solo','yellow'),press('rec1','Rec','red')])]),
    flow('Navigate tracks and banks','#strips',[
      step('Move one track','Press either Channel arrow to select the previous or next track.','Channel arrows also change the parent track in Send, Drum Mix, Inputs and Q-Link views.',[press('channelL channelR','Channel arrows')]),
      step('Move eight tracks','In Track view, press either Bank arrow to move to the previous or next bank of eight.','In Drum Mix these become pad banks; in insert editing they become parameter pages; in EQ they choose a Q-Link bank.',[press('bankL bankR','Bank arrows')])]),
    flow('Q-Link banks and mode','#plugins',[
      step('Open Q-Links','Press EQ. Turn the eight encoders to change Q-Links 1-8.','The controls follow the active MPC Q-Link mode and its assignments.',[press('eq','EQ'),turn('k1 k2 k3 k4 k5 k6 k7 k8','Encoders 1-8')]),
      step('Choose the other eight','Press a Bank arrow to switch between Q-Links 1-8 and 9-16.','The same eight encoders now operate the chosen Q-Link bank.',[press('bankL bankR','Bank arrows')]),
      step('Change Q-Link mode','Hold Shift and press a Bank arrow to change the MPC Q-Link mode. Release Shift when finished.','This changes the MPC mode, rather than just moving between the two banks of eight.',[hold('shift','Shift'),press('bankL bankR','Bank arrows')]),
      step('Use the faders','Press Flip to put Q-Links on the faders. Press Flip again to swap back.','Use the eight channel faders for the current Q-Link bank.',[press('flip','Flip'),move('f1 f2 f3 f4 f5 f6 f7 f8','Channel faders')])]),
    flow('Sends and returns','#views',[
      step('Open Sends','Select a track, then press Send.','The selected track is the parent of the send view. MIDI tracks do not provide pan or sends.',[press('select1','Track Select'),press('send','Send')]),
      step('Adjust the mix','Move faders 1-4 for the selected track\'s four sends. Turn encoders 1-4 for the return levels.','The lit Select identifies the parent track. Use Channel arrows to change that track.',[move('f1 f2 f3 f4','Send faders 1-4'),turn('k1 k2 k3 k4','Return encoders 1-4')])]),
    flow('Edit insert effects','#plugins',[
      step('Choose an insert','Press Plug-In, then push an occupied slot\'s encoder, 1-4, to open that insert.','An empty slot opens Add Effect on the MPC. Finish choosing the effect on the MPC.',[press('plugin','Plug-In'),press('k1 k2 k3 k4','Push a slot encoder')]),
      step('Edit parameters','Turn a parameter encoder to adjust it. Push an encoder to toggle a two-state parameter.','Bank arrows move through parameter pages. Press Plug-In to return to the slot list.',[turn('k1 k2 k3 k4 k5 k6 k7 k8','Parameter encoders'),press('bankL bankR','Parameter pages')]),
      step('Bypass an insert','In the slot list, hold Shift and push that slot\'s encoder.','The insert switches between active and bypassed. Release Shift after the press.',[hold('shift','Shift'),press('k1','Push slot encoder')]),
      step('Replace an effect','While editing an insert, hold Shift and press Plug-In.','Replace Effect opens on the MPC. Finish there. Synth instances do not expose parameter pages through this view.',[hold('shift','Shift'),press('plugin','Plug-In')])]),
    flow('Mix drum pads','#views',[
      step('Open Drum Mix','Select a Drum track, then press Aux.','The channel strips now control pads in the selected Drum program. Press Aux again to leave.',[press('select1','Drum track Select'),press('aux','Aux')]),
      step('Mix a pad','Move a fader for pad volume and turn its encoder for pad pan. Press Mute or Solo for that pad.','Pad names, colours, meters, Select and record arm are unavailable in Drum Mix.',[move('f1','Pad volume'),turn('k1','Pad pan'),press('mute1','Pad Mute','green'),press('solo1','Pad Solo','yellow')]),
      step('Reach every pad','Press Bank arrows to move through pads A01-H16. Channel arrows change the parent track.','Each bank places eight pads on the eight strips.',[press('bankL bankR','Pad bank'),press('channelL channelR','Parent track')])]),
    flow('Mixer views and track types','#views',[
      step('Filter the mixer','Choose MIDI Tracks, Audio Tracks, Audio Inst, Busses or Outputs. Global View or User restores all channel types.','These buttons change which channel types appear on the strips.',[press('midi audio audioinst busses outputs global user','Mixer filters')]),
      step('Show returns','Hold Shift and press Aux to show returns.','Release Shift after the press. Aux without Shift opens Drum Mix.',[hold('shift','Shift'),press('aux','Aux')]),
      step('Add a track','Press Inst (Instrument) to open Add Track.','The chooser opens on the MPC; choose the type there.',[press('instrument','Inst')]),
      step('Change a track type','Hold Shift and press Track to open the selected track\'s type chooser.','The chooser opens on the MPC; choose the type there.',[hold('shift','Shift'),press('track','Track')])]),
    flow('Inputs and outputs','#views',[
      step('Open the I/O view','Press Inputs for the selected track.','Faders keep track volume. Unavailable fields show dashes.',[press('inputs','Inputs')]),
      step('Choose routing','Turn encoders 1-8 to step through the available choices.','1 Monitor; 2 MIDI In Port; 3 MIDI In Channel; 4 Send To Track; 5 Audio Out; 6 MIDI Out Port; 7 MIDI Out Channel; 8 Audio In.',[turn('k1 k2 k3 k4 k5 k6 k7 k8','Routing encoders')])]),
    flow('Playback and recording','#transport',[
      step('Play and stop','Press Play to start from the current position. Press Stop to stop; press it again while stopped to return to the start.','Rewind and Fast Fwd move by beats. Hold either to repeat.',[press('play','Play','green'),press('stop','Stop'),press('rew ff','Rewind / Fast Fwd')]),
      step('Record, loop, click','Record toggles MPC recording. Cycle switches looping; Click switches the metronome.','The X-Touch position display follows bars, beats and ticks.',[press('record','Record','red'),press('cycle','Cycle'),press('click','Click')])]),
    flow('Jog and Grid note selection','#jog',[
      step('Scrub through time','With Scrub lit, turn the jog wheel to move by beats. Hold Shift while turning to move by ticks.','Scrub is the default after project load or controller reconnect. A hard flick may keep moving briefly.',[press('scrub','Scrub'),turn('jog','Jog wheel'),hold('shift','Shift (ticks)')]),
      step('Use the data wheel','Press Scrub so its LED is off. Turn the jog wheel to step the selected MPC control.','In the Grid, hold Shift and turn the jog wheel to select the previous or next note.',[press('scrub','Scrub off'),hold('shift','Shift (Grid notes)'),turn('jog','Jog wheel')]),
      step('Use the cursor keys','In scrub mode, the arrows move the cursor; Zoom switches them to zoom controls.','In data-wheel mode, Up/Down step values and Left/Right select controls. Zoom has no action there.',[press('up down left right zoom','Cursor / Zoom cluster')])]),
    flow('Save, undo and automation','#transport',[
      step('Save your work','Press Save. Press Undo to undo the last action.','An unnamed project opens the normal MPC save dialog. Enter and Cancel work where MPC popups accept them.',[press('save','Save'),press('undo','Undo'),press('enter cancel','Enter / Cancel')]),
      step('Redo','Hold Shift and press Undo to redo.','Release Shift after the press.',[hold('shift','Shift'),press('undo','Undo')]),
      step('Choose automation','Press Read/Off for Automation Read, Write for Automation Write, or Group for Automation Off.','Use the labelled button for the mode you want.',[press('read write group','Automation buttons')])]),
    flow('Faders and strip displays','#strips',[
      step('Swap controls','Press Flip to exchange fader and encoder assignments in the current view.','In insert parameter pages, parameters move to faders and track volumes move to encoders.',[press('flip','Flip')]),
      step('Motor movement','Hold Shift and press Flip to turn motor faders on or off.','Release Shift after the press.',[hold('shift','Shift'),press('flip','Flip')]),
      step('Names or values','Press Name/Value to switch the scribble strips between names and values.','Editing a control briefly displays its value.',[press('name','Name/Value')])])
  ];
  const mixerMode = step('Choose DAW Mixer','Press Mode, choose the button labelled DAW Mixer, then press Mode again.','The illustration highlights Mode. Use the labels shown on the XL3 to choose its mode.',[press('mode','Mode')]);
  const controlMode = step('Choose DAW Control','Press Mode, choose the button labelled DAW Control, then press Mode again.','The top eight encoders control the selected Q-Link or effect page. The middle and bottom rows are unassigned.',[press('mode','Mode')]);
  const xl = [
    flow('Track volume and pan','#mixer',[mixerMode,
      step('Mix the eight tracks','Move a fader to set its track\'s volume. Turn the bottom encoder in the same column to pan.','Faders use pickup: cross the current MPC value before the fader takes control. Encoders act relatively.',[move('f1','Track fader'),turn('k17','Bottom encoder')]),
      step('Change track banks','Press the Track arrows to move to the previous or next eight tracks.','The display labels and controls follow the current bank.',[press('trackL trackR','Track arrows')])]),
    flow('Select, mute, solo and arm','#mixer',[mixerMode,
      step('Solo a track','Press Solo/Arm until the upper row is in Solo mode, then press a track button in that row.','The upper row toggles Solo for the eight tracks.',[press('soloarm','Choose Solo'),press('upper1','Track Solo','yellow')]),
      step('Arm a track','Press Solo/Arm until the upper row is in Record Arm mode, then press a track button in that row.','Record arm is available only for supported tracks.',[press('soloarm','Choose Record Arm'),press('upper1','Track Arm','red')]),
      step('Mute a track','Press Mute/Select until the lower row is in Mute mode, then press a track button in that row.','The lower row toggles Mute for the eight tracks.',[press('muteselect','Choose Mute'),press('lower1','Track Mute','green')]),
      step('Select a track','Press Mute/Select until the lower row is in Select mode, then press the track button in that row.','The selected MPC track changes. Hardware selection LEDs are green; blue here identifies the Select action.',[press('muteselect','Choose Select'),press('lower1','Track Select','blue')]),
      step('Select with Shift','Hold Shift and press a track\'s lower-row button.','A Drum track opens its Pad mixer. Any other track returns to Track mixing. While Shift is held, the lower row represents tracks.',[hold('shift','Shift'),press('lower1','Track lower button','blue')])]),
    flow('Track sends','#mixer',[mixerMode,
      step('Send 1 and Send 2','Turn the top encoder for Send 1. Turn the middle encoder for Send 2 of the same track.','These are the two send rows in DAW Mixer. Unavailable controls stay dark.',[turn('k1','Top row: Send 1'),turn('k9','Middle row: Send 2')])]),
    flow('Q-Link banks and mode','#qlinks',[controlMode,
      step('Choose a bank','Use the Page arrows to select Q-Links 1-8 or Q-Links 9-16.','The Page arrows cycle through two Q-Link banks, followed by four effect-slot pages.',[press('pageUp pageDown','Page arrows')]),
      step('Turn Q-Links','Turn the top row of eight encoders for the selected Q-Link bank. Choose Q-Link mode and assignments on the MPC.','The display shows Q-Link numbers and current values. Page arrows do not change the MPC Q-Link mode.',[turn('k1 k2 k3 k4 k5 k6 k7 k8','Top eight encoders')])]),
    flow('Insert effect parameters','#qlinks',[controlMode,
      step('Choose an effect page','Use Page arrows past the two Q-Link banks to reach effect slots 1-4.','There is one page per effect slot, in the current MPC context.',[press('pageUp pageDown','Page arrows')]),
      step('Edit the effect','Turn the top row to change the first eight available parameters.','The display shows available parameter names and values. Faders retain the current Track or Pad mixer target.',[turn('k1 k2 k3 k4 k5 k6 k7 k8','Top eight encoders')])]),
    flow('Open the Drum Pad mixer','#pads',[mixerMode,
      step('Choose the Drum track','Hold Shift and press the Drum track\'s lower-row button.','The selected Drum program becomes the Pad mixer target.',[hold('shift','Shift'),press('lower1','Drum track lower button','blue')]),
      step('Return to Track mixing','Hold Shift and press Solo/Arm to switch between Track and Pad mixing.','This requires a selected Drum program. Returning to Track mixing restores the previous Solo/Arm and Mute/Select choices.',[hold('shift','Shift'),press('soloarm','Solo/Arm')])]),
    flow('Mix and bank drum pads','#pads',[
      step('Start in Pad mixing','In DAW Mixer, hold Shift and press the Drum track\'s lower-row button. Then release Shift.','Pad mixing follows the selected Drum program.',[hold('shift','Shift'),press('lower1','Drum track lower button','blue')]),
      step('Set pad volume and pan','Move a fader for pad volume. Turn the bottom encoder for pad pan.','The top and middle encoder rows have no pad sends. Pad selection and record arm are unavailable.',[move('f1','Pad fader'),turn('k17','Pad pan')]),
      step('Solo and mute pads','Press an upper-row button for Solo or a lower-row button for Mute.','In Pad mixing these rows always control Solo and Mute.',[press('upper1','Pad Solo','yellow'),press('lower1','Pad Mute','green')]),
      step('Reach all 128 pads','Press Track arrows to bank through pads A01-H16, eight at a time.','Fader labels identify the pads. Q-Link and effect pages remain accessible in DAW Control.',[press('trackL trackR','Pad bank arrows')])]),
    flow('Playback and recording','#mixer',[
      step('Play, pause, resume','Press Play to start playback. Press it again to pause in place, and again to resume.','Play does not perform a separate return-to-start gesture.',[press('play','Play','green')]),
      step('Record','Press Record to control MPC transport recording.','Track record arm is a separate action on the upper button row in Track mixing.',[press('record','Record','red')])]),
    flow('Preview a control and read feedback','#feedback',[
      step('Preview without editing','Hold Shift and turn a control to preview its name and value.','An unavailable encoder shows a dash. Release Shift before editing the value.',[hold('shift','Shift'),turn('k1','Example encoder')]),
      step('Read the display','Turn an available encoder or move a fader to see its name and value on the XL3 display.','Values use a 0-127 scale and follow changes on the MPC. Knob lights indicate availability, not a value ring.',[turn('k1','Example encoder'),move('f1','Example fader')])])
  ];
  const flows = device === 'xl3' ? xl : xt;
  function el(tag,attrs={},parent) { const e=document.createElement(tag); Object.entries(attrs).forEach(([k,v])=>e.setAttribute(k,v)); if(parent)parent.append(e);return e; }
  function svgEl(tag,attrs={},parent) { const e=document.createElementNS(NS,tag); Object.entries(attrs).forEach(([k,v])=>e.setAttribute(k,v)); if(parent)parent.append(e);return e; }
  function text(x,y,value,parent,cls='') { const e=svgEl('text',{x,y,class:cls},parent);value.split('|').forEach((line,i)=>{const s=svgEl('tspan',{x,dy:i?11:0},e);s.textContent=line;});return e; }
  const controls=new Map();
  function key(id,label,x,y,w=43,h=25) {
    const g=svgEl('g',{'data-control':id},hardware);controls.set(id,g);
    svgEl('rect',{x:x-3,y:y-3,width:w+6,height:h+6,rx:5,class:'control-halo'},g);
    svgEl('rect',{x,y,width:w,height:h,rx:4,class:'control-face'},g);
    text(x+w/2,y+(label.includes('|')?h/2-2:h/2+4),label,g,w<41?'control-label compact':'control-label');
  }
  function knob(id,x,y,label,radius=19) {
    const g=svgEl('g',{'data-control':id},hardware);controls.set(id,g);
    svgEl('circle',{cx:x,cy:y,r:radius+6,class:'control-halo'},g);
    svgEl('circle',{cx:x,cy:y,r:radius,class:'knob-face'},g);
    svgEl('line',{x1:x,y1:y-radius+4,x2:x,y2:y-radius+13,class:'knob-mark'},g);
    text(x,y+radius+19,label,g,'minor');
  }
  function fader(id,x,y,height=160,label='') {
    const g=svgEl('g',{'data-control':id},hardware);controls.set(id,g);
    svgEl('rect',{x:x-21,y:y-5,width:42,height:height+10,rx:5,class:'control-halo'},g);
    svgEl('rect',{x:x-3,y,width:6,height,rx:3,class:'slot'},g);
    for(let k=0;k<9;k++)svgEl('line',{x1:x-16,x2:x-9,y1:y+k*height/8,y2:y+k*height/8,stroke:'#68707b'},g);
    svgEl('rect',{x:x-16,y:y+height*.37,width:32,height:22,rx:3,class:'cap'},g);
    svgEl('line',{x1:x-12,x2:x+12,y1:y+height*.37+11,y2:y+height*.37+11,stroke:'#15181c','stroke-width':2},g);
    if(label)text(x,y+height+20,label,g,'minor');
  }
  root.innerHTML='<h2>Find the controls</h2><p>Choose a workflow, then select a step to see what to press, hold or turn. Nothing here controls your hardware.</p><div class="vg-picker"><label for="vg-workflow">I want to</label><select id="vg-workflow" aria-controls="vg-stage"></select></div><div class="vg-steps" role="group" aria-label="Workflow steps"></div><div class="vg-panel" id="vg-stage"><div class="vg-zoom" role="group" aria-label="Drawing detail"><span>Enlarge the drawing</span></div><div class="vg-drawing"></div><div class="vg-copy" aria-live="polite" aria-atomic="true"><div><p class="vg-workflow-title"></p><h3></h3><p class="vg-instruction"></p><div class="vg-actions"></div></div><div class="vg-result"><b>On the MPC / controller</b><p></p><a class="vg-reference">Full reference below</a></div></div></div><div class="vg-legend"><span><i></i>Press</span><span><i class="turn"></i>Turn / move</span><span><i class="hold"></i>Hold while using the other control</span></div><p class="vg-note">Highlights are instructions, not live LEDs. Select is blue, Mute green, Solo yellow and Record red. Held modifiers are pink. Use the detail buttons to read small labels.</p>';
  const svg=svgEl('svg',{viewBox:device==='xl3'?'0 0 628 602':'0 0 1040 700',role:'img','aria-labelledby':'vg-diagram-title',class:'vg-hardware','data-device':device},root.querySelector('.vg-drawing'));
  const title=svgEl('title',{id:'vg-diagram-title'},svg);
  const hardware=svgEl('g',{},svg);
  if(device==='xl3') {
    svgEl('rect',{x:2,y:2,width:624,height:598,rx:14,class:'shell'},hardware);
    text(185,30,'LAUNCH CONTROL XL 3',hardware,'device-name');
    svgEl('rect',{x:20,y:50,width:76,height:44,rx:3,class:'scribble'},hardware);text(58,75,'DISPLAY',hardware,'minor');
    text(55,123,'PAGE',hardware,'minor');key('pageUp','Up',24,134,30,24);key('pageDown','Dn',60,134,30,24);
    text(55,198,'TRACK',hardware,'minor');key('trackL','<',24,209,30,24);key('trackR','>',60,209,30,24);
    key('record','Record',24,292,64,30);key('play','Play',24,330,64,30);key('shift','Shift',24,397,64,30);key('mode','Mode',24,435,64,30);
    key('soloarm','Solo|Arm',24,497,64,37);key('muteselect','Mute|Select',24,548,64,37);
    [147,207,267,328,389,450,511,572].forEach((x,i)=>{
      [73,150,226].forEach((y,r)=>knob('k'+(r*8+i+1),x,y,String(i+1)));
      fader('f'+(i+1),x,298,165);key('upper'+(i+1),String(i+1),x-24,497,48,37);key('lower'+(i+1),String(i+1),x-24,548,48,37);
    });
  } else {
    svgEl('rect',{x:3,y:3,width:1034,height:694,rx:24,class:'shell'},hardware);
    for(let row=0;row<3;row++)for(let col=0;col<60;col++)svgEl('circle',{cx:40+col*16,cy:14+row*10,r:2.4,fill:'#111317'},hardware);
    
    for(let i=0;i<8;i++) {
      const x=80+i*72;
      svgEl('rect',{x:x-33,y:52,width:66,height:607,rx:8,class:'panel'},hardware);
      knob('k'+(i+1),x,89,String(i+1));
      svgEl('rect',{x:x-26,y:132,width:52,height:39,rx:3,class:'scribble'},hardware);text(x,156,'STRIP '+(i+1),hardware,'minor');
      for(let j=0;j<12;j++)svgEl('rect',{x:x-25,y:192+j*9,width:4,height:5,rx:1,fill:'#454d51'},hardware);
      ['rec','solo','mute','select'].forEach((name,r)=>key(name+(i+1),name==='select'?'Select':name[0].toUpperCase()+name.slice(1),x-12,191+r*33,40,25));
      fader('f'+(i+1),x,370,260,String(i+1));
    }
    text(340,352,'X-TOUCH',hardware,'device-name');
    fader('master',658,370,260,'MAIN');
    text(750,60,'ENCODER ASSIGN',hardware,'minor');
    [['track','Track'],['pan','Pan'],['eq','EQ'],['send','Send'],['plugin','Plug-In'],['instrument','Inst']].forEach(([id,l],i)=>key(id,l,636+i*41,76,35,25));
    text(936,87,'X-TOUCH',hardware,'device-name');
    key('name','Name/|Value',636,138,43,34);
    svgEl('rect',{x:696,y:138,width:298,height:34,rx:4,class:'scribble'},hardware);text(845,160,'BARS    BEATS    TICKS',hardware,'minor');
    key('global','Global|View',636,194,43,32);
    [['midi','MIDI|Tracks'],['inputs','Inputs'],['audio','Audio|Tracks'],['audioinst','Audio|Inst'],['aux','Aux'],['busses','Busses'],['outputs','Outputs'],['user','User']].forEach(([id,l],i)=>key(id,l,695+i*38,194,34,32));
    text(845,247,'FUNCTION',hardware,'minor');
    for(let i=0;i<8;i++)key('fn'+i,'F'+(i+1),695+i*38,257,34,24);
    key('flip','Flip',636,296,43,30);
    text(730,302,'MODIFY',hardware,'minor');text(842,302,'AUTOMATION',hardware,'minor');text(958,302,'UTILITY',hardware,'minor');
    [['shift','Shift'],['option','Option'],['control','Control'],['alt','Alt']].forEach(([id,l],i)=>key(id,l,695+(i%2)*43,313+Math.floor(i/2)*38,39,27));
    [['read','Read/|Off'],['write','Write'],['trim','Trim'],['touch','Touch'],['latch','Latch'],['group','Group']].forEach(([id,l],i)=>key(id,l,788+(i%3)*43,313+Math.floor(i/3)*38,39,27));
    [['save','Save'],['undo','Undo'],['cancel','Cancel'],['enter','Enter']].forEach(([id,l],i)=>key(id,l,922+(i%2)*43,313+Math.floor(i/2)*38,39,27));
    text(849,399,'TRANSPORT',hardware,'minor');
    [['marker','Marker'],['nudge','Nudge'],['cycle','Cycle'],['drop','Drop'],['replace','Replace'],['click','Click'],['solo','Solo']].forEach(([id,l],i)=>key(id,l,695+i*45,411,40,25));
    [['rew','Rewind'],['ff','Fast|Fwd'],['stop','Stop'],['play','Play'],['record','Record']].forEach(([id,l],i)=>key(id,l,695+i*63,450,56,32));
    text(747,504,'FADER BANK',hardware,'minor');key('bankL','<',700,513,36);key('bankR','>',754,513,36);
    text(747,559,'CHANNEL',hardware,'minor');key('channelL','<',700,568,36);key('channelR','>',754,568,36);
    key('up','Up',731,607,33,23);key('left','<',695,636,32,23);key('zoom','Zoom',731,636,33,23);key('right','>',768,636,32,23);key('down','Dn',731,666,33,23);
    knob('jog',905,598,'JOG',63);key('scrub','Scrub',962,514,43,27);

  }
  const views=device==='xl3' ? [['Whole device','0 0 628 602'],['Left controls','8 100 108 490'],['One strip','112 35 83 560']] : [['Whole device','0 0 1040 700'],['Channel strip','44 48 72 615'],['Right controls','626 48 390 646']];
  let zoomIndex=0;
  function setZoom(i) {
    zoomIndex=i;
    const box=views[i][1], dimensions=box.split(' ').map(Number);
    svg.setAttribute('viewBox',box);
    svg.style.maxWidth=i ? (650*dimensions[2]/dimensions[3])+'px' : device==='xl3'?'600px':'100%';
    zoomButtons.forEach((b,j)=>b.setAttribute('aria-pressed',String(i===j)));
  }
  const zoomButtons=views.map(([label],i)=>{const b=el('button',{type:'button','aria-pressed':String(i===0)},root.querySelector('.vg-zoom'));b.textContent=label;b.addEventListener('click',()=>setZoom(i));return b;});
  let printZoom=0;
  window.addEventListener('beforeprint',()=>{printZoom=zoomIndex;setZoom(0);});
  window.addEventListener('afterprint',()=>setZoom(printZoom));

  const selector=root.querySelector('select');flows.forEach((f,i)=>{const o=el('option',{value:i},selector);o.textContent=f.title;});
  let selected=0;
  function showStep(index) {
    if(zoomIndex!==0)setZoom(0);
    const f=flows[selected], s=f.steps[index];
    controls.forEach(g=>{g.setAttribute('class','');g.removeAttribute('style');});
    s.actions.forEach(a=>a.ids.forEach(id=>{const g=controls.get(id);if(!g)throw new Error('Unknown guide control: '+id);g.setAttribute('class','highlight '+a.verb);g.style.setProperty('--action-color',palette[a.color][0]);g.style.setProperty('--action-bg',palette[a.color][1]);}));
    root.querySelectorAll('.vg-steps button').forEach((b,i)=>{if(i===index)b.setAttribute('aria-current','step');else b.removeAttribute('aria-current');});
    root.querySelector('.vg-workflow-title').textContent=f.title;
    root.querySelector('.vg-copy h3').textContent=(index+1)+'. '+s.title;
    root.querySelector('.vg-instruction').textContent=s.instruction;
    root.querySelector('.vg-result p').textContent=s.result;
    root.querySelector('.vg-reference').href=f.reference;
    const actions=root.querySelector('.vg-actions');actions.replaceChildren();
    s.actions.forEach(a=>{const chip=el('span',{class:'vg-action'},actions);chip.style.setProperty('--action-color',palette[a.color][0]);chip.textContent=a.verb[0].toUpperCase()+a.verb.slice(1)+': '+a.label;});
    title.textContent=(device==='xl3'?'Launch Control XL3':'X-Touch')+' - '+f.title+' - '+s.title+'. '+s.actions.map(a=>a.verb+' '+a.label).join('; ');
  }
  function chooseFlow() {
    selected=Number(selector.value);const steps=root.querySelector('.vg-steps');steps.replaceChildren();
    flows[selected].steps.forEach((s,i)=>{const b=el('button',{type:'button','aria-controls':'vg-stage'},steps);const n=el('span',{},b);n.textContent=String(i+1).padStart(2,'0');b.append(document.createTextNode(s.title));b.addEventListener('click',()=>showStep(i));});
    if(zoomIndex!==0)zoomButtons[0].click();showStep(0);
  }
  selector.addEventListener('change',chooseFlow);chooseFlow();root.hidden=false;
})();
