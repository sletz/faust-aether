// Main thread: interface, presets, and performance events, without DSP computation.
// Sound is produced in aether-processor.js; controls and notes are sent through
// MessagePort, and statistics are received at a reduced update rate.
import {uiItems,controlKey} from './faust-wasm-dsp.js';
const $=id=>document.getElementById(id);
// Preset presentation; numeric values come from the JSON file shared by
// native and web renders, avoiding divergent preset banks.
const describe={
  nebula:['Nebula','PAD / ANALOG','A slow, luminous pad with an expansive sense of space.'],
  glass:['Glass','KEYS / FM','Shards of glass, blending FM bells with inharmonic overtones.'],
  acid:['Acid','SEQUENCE / BASS','A biting filter. Sixteen steps. A restless bass line.'],
  dark_brass:['Dark Brass','BRASS / HYBRID','A warm, dense breath shaped by a moving filter.'],
  orbital:['Orbital','TEXTURE / EXPERIMENTAL','Metal, ring modulation, and weightless echoes.'],
  crystal_sequence:['Crystal Sequence','SEQUENCE / DIGITAL','A crystalline arpeggio unfolding across a stereo space.']
};
const names={va_level:'Analog engine',fm_level:'FM engine',add_level:'Additive engine',sub_level:'Sub-octave',noise_level:'Pink noise',detune_cents:'Detune',morph:'Waveform',pulse_width:'Pulse width',fm_index:'FM index',fm_ratio:'FM ratio',fm_parallel:'Parallel FM',brightness:'Brightness',inharmonicity:'Inharmonicity',ring_mod:'Ring modulation',fold:'Wavefolding',drive:'Drive',amp_attack:'Amp attack',amp_decay:'Amp decay',amp_sustain:'Amp sustain',amp_release:'Amp release',filter_attack:'Filter attack',filter_decay:'Filter decay',filter_sustain:'Filter sustain',filter_release:'Filter release',mod_decay:'Mod envelope decay',cutoff:'Filter cutoff',resonance:'Resonance',filter_env_octaves:'Envelope → filter',keytrack:'Key tracking',filter_type:'Filter type',lfo1_hz:'LFO 1 / sine',lfo2_hz:'LFO 2 / triangle',macro1:'Macro 1',macro2:'Macro 2',velocity_cutoff_octaves:'Velocity → filter',stereo_spread:'Stereo unison',bpm:'Tempo',sequencer:'Sequencer enabled',sequence_octaves:'Interval multiplier',step_gate:'Step gate length',chorus_mix:'Chorus mix',chorus_rate:'Chorus rate',chorus_depth_ms:'Chorus depth',phaser_mix:'Phaser mix',phaser_rate:'Phaser rate',delay_mix:'Delay mix',delay_feedback:'Delay feedback',delay_beats:'Delay / beats',delay_tone:'Delay tone',reverb_mix:'Reverb mix',reverb_seconds:'Reverb time',predelay_ms:'Predelay',bass_db:'Bass',air_db:'Air',master_db:'Master volume',width:'Stereo width'};
const tabs=[['engines','Engines'],['filter','Filter & ADSR'],['modulation','Modulation'],['effects','Effects'],['sequence','Sequencer'],['advanced','All controls']];
const curated={engines:['va_level','fm_level','add_level','sub_level','morph','detune_cents','fm_index','fm_ratio','brightness','inharmonicity','fold','drive'],filter:['cutoff','resonance','filter_type','filter_env_octaves','amp_attack','amp_decay','amp_sustain','amp_release','filter_attack','filter_decay','filter_sustain','filter_release','keytrack','velocity_cutoff_octaves','mod_decay'],modulation:['lfo1_hz','lfo2_hz','macro1','macro2','stereo_spread'],sequence:['sequencer','bpm','step_gate','sequence_octaves']};
const sources=['Disabled','LFO 1','LFO 2','Filter env.','Mod env.','Velocity','Sequencer','Macro 1','Macro 2'];
const destinations=['Disabled','Pitch','Filter','FM index','Waveform','Pan','Amplitude'];
// Three separate layers: defs describes Faust widgets, values holds the
// current patch, and views maps controls to DOM elements in the visible tab.
const defs=new Map(), values=new Map(), views=new Map();
let patches={}, compiled=null, selected='nebula', activeTab='engines', octave=3, demoTimers=[];
const audio={context:null,node:null,analyser:null,running:false,busy:false};
let activeKeys=[];
// Several sources (MIDI, pointer, computer keyboard) can hold the same note.
// Release it only when the last source lets go to avoid premature note-offs.
const heldSources=new Map();
const leaf=key=>key.split('/').at(-1);
// Presets use unique short names; routes use qualified suffixes such as
// Route_2/depth to disambiguate repeated control names.
function resolve(name){if(defs.has(name))return name;return [...defs.keys()].find(k=>k.endsWith('/'+name)||k===name);}
function feedback(text,error=false){$('feedback').textContent=text;$('feedback').classList.toggle('error',error);}
function message(m){audio.node?.port.postMessage(m);}
function pretty(d,v){
  if(d.type==='checkbox')return v?'ON':'OFF';
  if(leaf(d.key)==='filter_type')return v<0.5?'Ladder':v<1.5?'Band-pass':'High-pass';
  if(d.max>=1000 && v>=1000)return (v/1000).toFixed(2)+' kHz';
  const unit=(d.meta||[]).find(m=>m.unit)?.unit||'';
  return Number(v).toFixed(d.step>=1?0:d.step>=0.1?1:2)+(unit?' '+unit:'');
}
// Shared entry point for sliders and presets: clamp in the DSP's units,
// update views and accessibility text, then send to the audio thread.
// Logarithmic views store log(value); the engine receives the actual value.
function setValue(name,value,send=true){
  const key=resolve(name), d=defs.get(key);
  if(!d||!Number.isFinite(Number(value)))return;
  value=Math.min(d.max,Math.max(d.min,Number(value)));
  values.set(key,value);
  for(const view of views.get(key)||[]){view.input.value=view.log?Math.log(value):value;if(d.type==='checkbox')view.input.checked=!!value;view.output.textContent=pretty(d,value);view.input.setAttribute('aria-valuetext',pretty(d,value));}
  if(send)message({type:'control',key,value});
  updateModified();
}
// Compare against the preset completed with JSON defaults. The tolerance
// absorbs float32 representation differences in initial widget values.
function updateModified(){
  if(!patches[selected])return;
  const changed=[...defs].some(([k,d])=>Math.abs(values.get(k)-(patches[selected][leaf(k)]??d.init))>1e-6);
  $('modified').hidden=!changed;
}
// Preset recall is complete: restore unspecified controls to their defaults
// so settings from the previously selected sound do not accidentally persist.
// Local storage only remembers the preset name and is optional.
function applyPreset(id){
  if(!patches[id])return;
  selected=id;
  for(const [key,d]of defs)setValue(key,patches[id][leaf(key)]??d.init);
  document.querySelectorAll('.preset-card').forEach(el=>{el.classList.toggle('active',el.dataset.preset===id);el.setAttribute('aria-pressed',String(el.dataset.preset===id));});
  $('patch-name').textContent=describe[id][0];$('patch-description').textContent=describe[id][2];
  $('modified').hidden=true;
  try{localStorage.setItem('aether-preset',id);}catch{}
}
// Build a control from JSON instead of duplicating its bounds in the UI.
// Wide, strictly positive ranges use logarithmic travel; display text and
// aria-valuetext remain in the parameter's musical units.
function makeControl(key){
  const d=defs.get(key), box=document.createElement('div');box.className='control';
  const top=document.createElement('div');top.className='control-top';
  const label=document.createElement('label'), output=document.createElement('output');
  const id='ctrl-'+key.replace(/[^a-z0-9]/gi,'-');label.htmlFor=id;label.textContent=names[leaf(key)]||leaf(key).replaceAll('_',' ');
  const input=document.createElement('input');input.id=id;
  const logarithmic=d.min>0 && (d.max/d.min>100) && !leaf(key).includes('ratio');
  if(d.type==='checkbox'){input.type='checkbox';input.checked=!!values.get(key);}
  else{input.type='range';input.min=logarithmic?Math.log(d.min):d.min;input.max=logarithmic?Math.log(d.max):d.max;input.step=logarithmic?'any':d.step||0.001;input.value=logarithmic?Math.log(values.get(key)):values.get(key);}
  output.textContent=pretty(d,values.get(key));output.htmlFor=id;
  input.setAttribute('aria-label',label.textContent);input.setAttribute('aria-valuetext',pretty(d,values.get(key)));
  input.addEventListener('input',()=>setValue(key,d.type==='checkbox'?Number(input.checked):logarithmic?Math.exp(Number(input.value)):Number(input.value)));
  top.append(label,output);box.append(top,input);
  if(!views.has(key))views.set(key,[]);views.get(key).push({input,output,log:logarithmic});
  return box;
}
// Menu indices must match source() and mod() in aether.dsp.
// Each route receives its signed depth independently.
function buildMatrix(panel){
  const table=document.createElement('table');table.className='route-table';
  const head=table.createTHead().insertRow();for(const title of ['ROUTE','SOURCE','DESTINATION','DEPTH']){const th=document.createElement('th');th.textContent=title;head.append(th);}
  for(let i=0;i<8;i++){
    const row=table.insertRow();row.insertCell().textContent=String(i+1).padStart(2,'0');
    for(const [param,options]of [['source',sources],['destination',destinations]]){
      const key=resolve(`Route_${i}/${param}`),sel=document.createElement('select');sel.setAttribute('aria-label',`Route ${i+1} ${param}`);
      options.forEach((name,n)=>{const op=document.createElement('option');op.value=n;op.textContent=name;sel.append(op);});sel.value=values.get(key);
      const output=document.createElement('output');
      views.set(key,[{input:sel,output,log:false}]);sel.onchange=()=>setValue(key,Number(sel.value));row.insertCell().append(sel);
    }
    const cell=row.insertCell(), key=resolve(`Route_${i}/depth`),input=document.createElement('input'),output=document.createElement('output');
    input.type='range';input.min=-1;input.max=1;input.step=.001;input.value=values.get(key);input.setAttribute('aria-label',`Route ${i+1} depth`);output.textContent=Number(values.get(key)).toFixed(2);
    views.set(key,[{input,output,log:false}]);input.oninput=()=>setValue(key,Number(input.value));cell.append(input,output);
  }
  panel.append(table);
}
// Rebuild views without changing the patch or audio state. Routes and sixteen
// steps have dedicated layouts and are not duplicated in the Advanced tab.
function showTab(id){
  activeTab=id;views.clear();
  document.querySelectorAll('.tab').forEach(b=>{b.classList.toggle('active',b.dataset.tab===id);b.setAttribute('aria-selected',String(b.dataset.tab===id));});
  const panel=$('control-panel');panel.replaceChildren();
  const header=document.createElement('div');header.className='panel-header';const title=document.createElement('h3');title.textContent='02 / '+tabs.find(t=>t[0]===id)[1].toUpperCase();
  const hint=document.createElement('p');hint.textContent=id==='modulation'?'8 routes · 9 sources · 6 destinations':id==='engines'?'Analog × FM × additive':'Every movement shapes the sound.';header.append(title,hint);panel.append(header);
  const grid=document.createElement('div');grid.className='control-grid';
  let keys=id==='effects'?[...defs.keys()].filter(k=>k.startsWith('Effects/')):id==='advanced'?[...defs.keys()]:curated[id].map(resolve);
  if(id==='advanced')keys=keys.filter(k=>!k.includes('/Steps/')&&!k.includes('/Route_'));
  keys.forEach(key=>{if(key)grid.append(makeControl(key));});panel.append(grid);
  if(id==='modulation'){grid.style.marginBottom='25px';buildMatrix(panel);}
  if(id==='sequence'){
    const steps=document.createElement('div');steps.className='sequence-grid';steps.style.marginTop='28px';
    for(let i=0;i<16;i++){const key=resolve('note_'+i);const control=makeControl(key);control.querySelector('label').textContent='STEP '+String(i+1).padStart(2,'0');steps.append(control);}panel.append(steps);
  }
}
async function json(url){const r=await fetch(url);if(!r.ok)throw Error(`Loading ${url}: HTTP ${r.status}`);return r.json();}
// Load and compile WASM before playing. Compiled modules are shared with
// AudioWorklet, but each DSP retains a private instance and memory.
async function loadModule(name){const [meta,bytes]=await Promise.all([json(`dsp/${name}.json`),fetch(`dsp/${name}.wasm`).then(r=>{if(!r.ok)throw Error(`Module ${name} is unavailable. Run make web.`);return r.arrayBuffer();})]);return{json:meta,module:await WebAssembly.compile(bytes)};}
// Create/resume AudioContext from a click to respect autoplay rules.
// busy prevents concurrent clicks while loading the processor.
// Suspend keeps the context reusable after panic clears its notes.
async function startAudio(){
  if(audio.busy)return;audio.busy=true;$('audio-toggle').disabled=true;
  try{
    if(!audio.context){
      const Context=window.AudioContext||window.webkitAudioContext;if(!Context)throw Error('Web Audio is unavailable in this browser.');
      audio.context=new Context({latencyHint:'interactive'});await audio.context.resume();
      await audio.context.audioWorklet.addModule('./aether-processor.js');
      // No microphone: zero inputs. Pass the current patch to the constructor
      // so the first note does not use defaults from a different preset.
      audio.node=new AudioWorkletNode(audio.context,'aether',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{...compiled,polyphony:Number($('polyphony').value),controls:Object.fromEntries(values)}});
      // Display engine telemetry without running DSP here. Keys held by the
      // processor complement those tracked through local input events.
      audio.node.port.onmessage=({data:m})=>{if(m.type==='error')feedback(m.message,true);if(m.type==='stats'){
        $('voices-stat').innerHTML=`${m.active} <em>/ ${$('polyphony').value}</em>`;
        $('load-stat').innerHTML=`${m.load.toFixed(0)} <em>%</em>`;
        $('level-meter').style.width=Math.min(100,Math.sqrt(m.peak)*100)+'%';activeKeys=m.keys;paintKeys();
        if(m.invalid)feedback('The engine detected non-finite output. Use Stop all.',true);
      }};
      audio.node.onprocessorerror=()=>feedback('The audio engine stopped. Reload the page.',true);
      // Analyze the outgoing stream without replacing it: the same signal feeds
      // the display and audio destination. The visual gain in draw does not change sound.
      audio.analyser=audio.context.createAnalyser();audio.analyser.fftSize=2048;
      audio.node.connect(audio.analyser);audio.analyser.connect(audio.context.destination);
      $('rate-stat').innerHTML=`${(audio.context.sampleRate/1000).toFixed(1)} <em>kHz</em>`;
    }
    if(audio.running){panic();await audio.context.suspend();audio.running=false;}
    else{await audio.context.resume();audio.running=true;feedback('Ready to play. The keyboard and presets work in real time.');}
    $('audio-toggle').innerHTML=audio.running?'Suspend audio <span>Ⅱ</span>':'Enable audio <span>↗</span>';
    $('engine-status').textContent=audio.running?'Engine running':'Engine idle';$('engine-status').classList.toggle('live',audio.running);
  }catch(e){feedback(e.message,true);if(audio.context&&!audio.node){await audio.context.close();audio.context=null;}}
  finally{audio.busy=false;$('audio-toggle').disabled=false;}
}
// Use heldSources to implicitly count holders of each note.
// An additional press at the same pitch does not allocate another voice.
function press(source,key,velocity=.8){
  if(!audio.running){feedback('Enable audio to play.');return;}
  if(heldSources.has(source))return;
  const wasHeld=[...heldSources.values()].includes(key);heldSources.set(source,key);
  if(!wasHeld)message({type:'noteOn',key,velocity});paintKeys();
}
function release(source){const key=heldSources.get(source);if(key===undefined)return;heldSources.delete(source);if(![...heldSources.values()].includes(key))message({type:'noteOff',key});paintKeys();}
function paintKeys(){document.querySelectorAll('.piano-key').forEach(b=>b.classList.toggle('active',activeKeys.includes(Number(b.dataset.note))||[...heldSources.values()].includes(Number(b.dataset.note))));}
// Cancel demo chord timers too; otherwise a deferred note-on could sound
// after Stop all, when the user expects silence.
function panic(){demoTimers.forEach(clearTimeout);demoTimers=[];heldSources.clear();activeKeys=[];message({type:'panic'});paintKeys();$('level-meter').style.width='0';}
// Two octaves plus the final C. White keys define the grid; black keys are
// positioned using the number of preceding white keys. Pointer capture
// preserves note-off even when a finger leaves the key.
function keyboard(){
  const box=$('keyboard');box.replaceChildren();let whites=0;const first=(octave+1)*12;
  for(let i=0;i<=24;i++)if(![1,3,6,8,10].includes(i%12))whites++;
  let whiteIndex=0;
  for(let i=0;i<=24;i++){
    const key=first+i,black=[1,3,6,8,10].includes(key%12),button=document.createElement('button');
    button.type='button';button.className='piano-key '+(black?'black':'white');button.dataset.note=key;
    const name=['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'][key%12]+(Math.floor(key/12)-1);button.setAttribute('aria-label',name);
    if(black)button.style.left=`calc(${whiteIndex/whites*100}% - 1.825%)`;else whiteIndex++;
    if(key%12===0){const s=document.createElement('span');s.textContent='C'+(Math.floor(key/12)-1);button.append(s);}
    button.onpointerdown=e=>{e.preventDefault();button.setPointerCapture(e.pointerId);press('pointer-'+e.pointerId,key,.8);};
    button.onpointerup=e=>release('pointer-'+e.pointerId);button.onpointercancel=e=>release('pointer-'+e.pointerId);button.onlostpointercapture=e=>release('pointer-'+e.pointerId);box.append(button);
  }
  $('octave').value=octave;paintKeys();
}
// KeyboardEvent.code identifies a physical position, keeping notes stable
// across AZERTY/QWERTY layouts. Ignore repeats and editable fields.
// On focus loss, release local sources; a held MIDI note remains valid.
const keyOffsets={KeyA:0,KeyW:1,KeyS:2,KeyE:3,KeyD:4,KeyF:5,KeyT:6,KeyG:7,KeyY:8,KeyH:9,KeyU:10,KeyJ:11,KeyK:12,KeyO:13,KeyL:14,KeyP:15,Semicolon:16};
window.addEventListener('keydown',e=>{if(e.repeat||e.ctrlKey||e.metaKey||e.altKey||/INPUT|SELECT|TEXTAREA/.test(document.activeElement?.tagName))return;if(keyOffsets[e.code]!==undefined){e.preventDefault();press('key-'+e.code,(octave+1)*12+keyOffsets[e.code]);}if(e.code==='Escape')panic();});
window.addEventListener('keyup',e=>release('key-'+e.code));window.addEventListener('blur',()=>{for(const source of [...heldSources.keys()])if(!source.startsWith('midi-'))release(source);});
$('octave-down').onclick=()=>{panic();octave=Math.max(1,octave-1);keyboard();};$('octave-up').onclick=()=>{panic();octave=Math.min(6,octave+1);keyboard();};
$('panic').onclick=panic;$('polyphony').onchange=()=>{panic();message({type:'polyphony',count:Number($('polyphony').value)});$('voices-stat').innerHTML=`0 <em>/ ${$('polyphony').value}</em>`;};
$('audio-toggle').onclick=startAudio;$('reset-patch').onclick=()=>applyPreset(selected);
// The chord uses the same press/release path as the keyboard. Dedicated
// demo source IDs keep its note-offs from cutting off manual playing.
$('play-demo').onclick=async()=>{
  if(!audio.running)await startAudio();if(!audio.running)return;panic();
  const notes=[48,55,60,63];notes.forEach((key,i)=>{demoTimers.push(setTimeout(()=>press('demo-'+i,key,.8),i*110));demoTimers.push(setTimeout(()=>release('demo-'+i),3300+i*110));});
};
// Request Web MIDI only from this button, without SysEx. Port and channel
// identify sources; a velocity-zero noteOn means noteOff. On disconnect,
// release that port's notes to prevent stuck notes.
$('midi-connect').onclick=async()=>{
  try{if(!navigator.requestMIDIAccess)throw Error('Web MIDI is unavailable. Try Chrome or Edge.');const access=await navigator.requestMIDIAccess({sysex:false});
    const attached=new Map();
    const sync=()=>{for(const [id,input]of attached)if(!access.inputs.has(id)){for(const source of [...heldSources.keys()])if(source.startsWith('midi-'+id+'-'))release(source);attached.delete(id);}
      for(const input of access.inputs.values()){attached.set(input.id,input);input.onmidimessage=({data:[status,key,velocity]})=>{const source=`midi-${input.id}-${status&15}-${key}`,type=status&240;if(type===144&&velocity>0)press(source,key,velocity/127);if(type===128||(type===144&&velocity===0))release(source);if(type===176&&(key===120||key===123))panic();};}
      $('midi-status').textContent=access.inputs.size?[...access.inputs.values()].map(i=>i.name).join(' · '):'No MIDI input detected';};sync();access.onstatechange=sync;$('midi-connect').disabled=true;
  }catch(e){feedback(e.message,true);}
};
// The oscilloscope updates at display rate, independently of audio quanta.
// Its ×4 magnification and clamping affect drawing only.
const canvas=$('scope'),ctx=canvas.getContext('2d'),samples=new Float32Array(2048);
function draw(){
  const w=canvas.width,h=canvas.height;ctx.clearRect(0,0,w,h);ctx.strokeStyle='#203026';ctx.lineWidth=1;
  for(let x=0;x<w;x+=50){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke();}for(let y=25;y<h;y+=25){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();}
  if(audio.analyser&&audio.running)audio.analyser.getFloatTimeDomainData(samples);else samples.fill(0);
  ctx.strokeStyle='#bbdbb2';ctx.lineWidth=1.7;ctx.beginPath();for(let i=0;i<1000;i++){const value=samples[i]*4;const x=i/999*w,y=h*.54-Math.max(-1,Math.min(1,value))*h*.32;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();requestAnimationFrame(draw);
}
// Discover the UI from both module JSON files, then apply the remembered
// preset. Faust buttons/checkboxes may omit min/max/init: provide 0..1 and
// init=0 to avoid sending undefined values into the engine.
// freq/gain/gate belong to the note engine, outside timbre presets.
async function init(){
  $('audio-toggle').disabled=true;
  try{
    const [voice,effects,p]=await Promise.all([loadModule('voice'),loadModule('effects'),json('../presets/patches.json')]);compiled={voice,effects};patches=p;
    for(const meta of [voice.json,effects.json])for(const d of uiItems(meta.ui)){const key=controlKey(d.address||'');if(!key||['freq','gain','gate'].includes(key)||d.type.includes('bargraph'))continue;const def={min:0,max:1,step:1,init:0,...d,key};defs.set(key,def);values.set(key,def.init);}
    Object.keys(patches).forEach((id,index)=>{const b=document.createElement('button');b.className='preset-card';b.dataset.preset=id;b.setAttribute('aria-pressed','false');b.innerHTML=`<span class="preset-index">${String(index+1).padStart(2,'0')}</span><span><strong>${describe[id][0]}</strong><small>${describe[id][1]}</small></span><span class="preset-arrow">↗</span>`;b.onclick=()=>applyPreset(id);$('preset-list').append(b);});
    for(const [id,label]of tabs){const b=document.createElement('button');b.className='tab';b.dataset.tab=id;b.textContent=label;b.setAttribute('role','tab');b.onclick=()=>showTab(id);$('tabs').append(b);}
    let initial='nebula';try{const saved=localStorage.getItem('aether-preset');if(patches[saved])initial=saved;}catch{}
    applyPreset(initial);showTab(activeTab);$('audio-toggle').disabled=false;
    // Expose a development inspection point; normal interaction uses the
    // widgets and event handlers declared above.
    window.aether={values,defs,applyPreset,setValue,audio,panic,get selected(){return selected;}};
  }catch(e){feedback(e.message+' Serve the project with make web-serve.',true);$('engine-status').textContent='Unable to load';}
}
keyboard();draw();init();
