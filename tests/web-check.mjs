import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {FaustWasmDsp,uiItems,controlKey} from '../web/faust-wasm-dsp.js';
import {AetherEngine} from '../web/aether-engine.js';
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const results=path.join(root,'test-results/web');fs.mkdirSync(results,{recursive:true});
const probe=process.env.FAUSTPROBE||'faustprobe';
function run(cmd,args){console.log('$',cmd,...args);const p=spawnSync(cmd,args,{encoding:'utf8'});process.stdout.write(p.stdout||'');process.stderr.write(p.stderr||'');assert.equal(p.status,0);return p.stdout;}
run(process.env.FAUST||'faust',['--version']);run(process.env.FAUSTRS||'faust-rs',['--version']);run(probe,['--version']);
async function load(name){return{json:JSON.parse(fs.readFileSync(path.join(root,'web/dsp',name+'.json'))),module:await WebAssembly.compile(fs.readFileSync(path.join(root,'web/dsp',name+'.wasm')))}}
const voice=await load('voice'),effects=await load('effects');
const patches=JSON.parse(fs.readFileSync(path.join(root,'presets/patches.json')));
const defs=new Map();for(const c of [voice,effects])for(const d of uiItems(c.json.ui))if(d.address)defs.set(controlKey(d.address),{min:0,max:1,step:1,init:0,...d});
function key(suffix){const matches=[...defs.keys()].filter(k=>k===suffix||k.endsWith('/'+suffix));assert.equal(matches.length,1,suffix);return matches[0];}
function settings(patch){return Object.fromEntries([...defs].filter(([k])=>!['freq','gain','gate'].includes(k)).map(([k,d])=>[k,patch[k.split('/').at(-1)]??d.init]));}
function wavRead(file){const b=fs.readFileSync(file);let p=12,fmt,data;while(p+8<=b.length){const n=b.readUInt32LE(p+4),tag=b.toString('ascii',p,p+4);if(tag==='fmt ')fmt={type:b.readUInt16LE(p+8),channels:b.readUInt16LE(p+10),bits:b.readUInt16LE(p+22)};if(tag==='data')data=b.subarray(p+8,p+8+n);p+=8+n+(n%2);}assert.equal(fmt.type,3);const stride=fmt.bits/8;return Array.from({length:fmt.channels},(_,c)=>Float64Array.from({length:data.length/stride/fmt.channels},(_,i)=>stride===8?data.readDoubleLE((i*fmt.channels+c)*stride):data.readFloatLE((i*fmt.channels+c)*stride)));}
function wavWrite(file,channels){const n=channels[0].length,nc=channels.length,b=Buffer.alloc(44+n*nc*8);b.write('RIFF');b.writeUInt32LE(b.length-8,4);b.write('WAVEfmt ',8);b.writeUInt32LE(16,16);b.writeUInt16LE(3,20);b.writeUInt16LE(nc,22);b.writeUInt32LE(44100,24);b.writeUInt32LE(44100*nc*8,28);b.writeUInt16LE(nc*8,32);b.writeUInt16LE(64,34);b.write('data',36);b.writeUInt32LE(n*nc*8,40);for(let i=0;i<n;i++)for(let c=0;c<nc;c++)b.writeDoubleLE(channels[c][i],44+(i*nc+c)*8);fs.writeFileSync(file,b);}
function render(dsp,n,input,fn=()=>{}){const outs=Array.from({length:dsp.outputs},()=>new Float64Array(n));for(let t=0;t<n;t+=128){const frames=Math.min(128,n-t);fn(t);for(let c=0;c<dsp.inputs;c++)dsp.input(c).set(input[c].subarray(t,t+frames));dsp.compute(frames);for(let c=0;c<dsp.outputs;c++)outs[c].set(dsp.output(c).subarray(0,frames),t);}return outs;}
function compare(a,b){assert.equal(a.length,b.length);let max=0,peak=0;for(let c=0;c<a.length;c++){assert.equal(a[c].length,b[c].length);for(let i=0;i<a[c].length;i++){assert.ok(Number.isFinite(a[c][i])&&Number.isFinite(b[c][i]));max=Math.max(max,Math.abs(a[c][i]-b[c][i]));peak=Math.max(peak,Math.abs(b[c][i]));}}assert.ok(peak>1e-6,'comparison must be audible');assert.ok(max<5e-5+peak*0.001,`difference ${max} / peak ${peak}`);return{max_abs_difference:max,reference_peak:peak,frames:a[0].length,channels:a.length};}
// The checker must reject a changed sample before its verdicts are trusted.
assert.throws(()=>compare([[.1,.2]],[[.1,.8]]));
const report=[];
const frames=22050,release=11008;
const v=new FaustWasmDsp(voice,44100,128);v.set('freq',220);v.set('gain',.7);
const rendered=render(v,frames,[],t=>v.set('gate',t<release?1:0));
const reference=path.join(results,'voice-native.wav');run(probe,['--single','-I',root,'--in','zero','--set','freq=220','--set','gain=0.7','--at','0','gate=1','--at',String(release),'gate=0','-n',String(frames),'--out',reference,path.join(root,'aether.dsp')]);
report.push({name:'voice-wasm-vs-native',...compare(rendered,wavRead(reference))});
const input=path.join(results,'effect-input.wav');wavWrite(input,rendered);
const fx=new FaustWasmDsp(effects,44100,128),wet=render(fx,frames,rendered);
const effectRef=path.join(results,'effects-native.wav');run(probe,['--single','-I',root,'--in','file:'+input,'-n',String(frames),'--out',effectRef,path.join(root,'effects.dsp')]);
report.push({name:'effect-wasm-vs-native',...compare(wet,wavRead(effectRef))});
const left=new Float32Array(128),right=new Float32Array(128);
for(const [name,patch]of Object.entries(patches)){
  const engine=new AetherEngine(voice,effects,44100,4,settings(patch));
  [48,55,60,63].forEach(n=>engine.noteOn(n,.8));
  let energy=0,peak=0;
  for(let b=0;b<256;b++){
    if(b===64){engine.noteOn(67,.9);engine.noteOff(67);} // steal + same-quantum note-off
    if(b===128)[48,55,60,63].forEach(n=>engine.noteOff(n));
    const stats=engine.compute(128,left,right);assert.equal(stats.invalid,0);peak=Math.max(peak,stats.peak);
    for(let i=0;i<128;i++)energy+=left[i]**2+right[i]**2;
  }
  assert.ok(energy>1e-5&&peak<=.921,name);
  engine.panic();const s=engine.compute(128,left,right);assert.equal(s.active,0);assert.ok(s.peak<1e-10);
  report.push({name:'polyphonic-'+name,frames:32768,voices:4,peak,energy});
}
// Every preset override resolves to exactly one UI control.
for(const patch of Object.values(patches))for(const k of Object.keys(patch))key(k);
fs.writeFileSync(path.join(results,'summary.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report,null,2));console.log('PASS: 2 wasm/native comparisons, 6 polyphonic presets, voice stealing, panic, negative control');
