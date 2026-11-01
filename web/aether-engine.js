import { FaustWasmDsp } from './faust-wasm-dsp.js';

// The same engine is exercised offline by tests/web-check.mjs and in AudioWorklet.
export class AetherEngine {
  constructor(voice, effects, sampleRate, count = 4, controls = {}) {
    this.compiled = voice;
    this.sampleRate = sampleRate;
    this.blockSize = 128;
    this.values = new Map(Object.entries(controls).filter(([,value])=>Number.isFinite(value)));
    this.effects = this.makeDsp(effects);
    if (this.effects.inputs !== 2 || this.effects.outputs !== 2) throw Error('Stereo effect required');
    this.voices = [];
    this.counter = 0;
    this.setPolyphony(count);
  }
  makeDsp(compiled) {
    const dsp = new FaustWasmDsp(compiled, this.sampleRate, this.blockSize);
    for (const [key,value] of this.values) dsp.set(key,value);
    return dsp;
  }
  setPolyphony(count) {
    if (!Number.isInteger(count) || count < 1 || count > 12) return;
    while (this.voices.length < count) {
      const dsp = this.makeDsp(this.compiled);
      if (dsp.inputs !== 0 || dsp.outputs !== 2) throw Error('Stereo generator required');
      this.voices.push({dsp,active:false,held:false,key:-1,age:0,quiet:0,pending:null,level:0});
    }
    this.voices.length = count;
  }
  set(key,value) {
    if (!Number.isFinite(value)) return;
    this.values.set(key,value);
    for (const v of this.voices) v.dsp.set(key,value);
    this.effects.set(key,value);
  }
  start(v,key,velocity,release = false) {
    v.dsp.set('freq',440*2**((key-69)/12));
    v.dsp.set('gain',Math.min(1,Math.max(0.01,velocity)));
    v.dsp.set('gate',1);
    Object.assign(v,{key,held:true,active:true,age:this.counter++,quiet:0,pending:null,
                     releaseAfterBlock:release,justStarted:true});
  }
  noteOn(key,velocity = 0.8) {
    if (!Number.isInteger(key) || key < 0 || key > 127 || !Number.isFinite(velocity)) return;
    if (velocity <= 0) { this.noteOff(key); return; }
    let v = this.voices.find(v => v.active && v.key === key);
    if (!v) v = this.voices.find(v => !v.active && !v.pending);
    if (!v) {
      const available = this.voices.filter(v => !v.pending);
      if (!available.length) return;
      const released = available.filter(v => !v.held);
      v = (released.length ? released : available).reduce((a,b) =>
        released.length ? (a.level < b.level ? a : b) : (a.age < b.age ? a : b));
    }
    if (v.active) v.pending = {key,velocity,release:false};
    else this.start(v,key,velocity);
  }
  noteOff(key) {
    for (const v of this.voices) {
      if (v.pending?.key === key) v.pending.release = true;
      else if (v.active && v.key === key && v.held) {
        if (v.justStarted) v.releaseAfterBlock = true;
        else { v.held = false; v.dsp.set('gate',0); }
      }
    }
  }
  panic() {
    for (const v of this.voices) {
      v.dsp.exports.instanceClear(0); v.dsp.set('gate',0);
      Object.assign(v,{active:false,held:false,key:-1,pending:null,quiet:0,justStarted:false,releaseAfterBlock:false});
    }
    this.effects.exports.instanceClear(0);
  }
  compute(frames, left, right) {
    const l = this.effects.input(0), r = this.effects.input(1);
    l.fill(0); r.fill(0);
    for (const v of this.voices) {
      if (!v.active) continue;
      v.dsp.compute(frames);
      const vl=v.dsp.output(0), vr=v.dsp.output(1);
      let level=0;
      for (let i=0;i<frames;i++) {
        const fade = v.pending ? 1-i/Math.max(1,frames-1) : 1;
        l[i] += vl[i]*fade; r[i] += vr[i]*fade;
        level = Math.max(level,Math.abs(vl[i]),Math.abs(vr[i]));
      }
      v.level = level;
      if (v.pending) {
        const next=v.pending;
        v.dsp.exports.instanceClear(0);
        this.start(v,next.key,next.velocity,next.release);
        continue;
      }
      v.justStarted = false;
      if (v.releaseAfterBlock) { v.releaseAfterBlock=false; v.held=false; v.dsp.set('gate',0); }
      if (!v.held) {
        v.quiet = level < 0.00001 ? v.quiet+frames : 0;
        if (v.quiet > this.sampleRate*0.2) v.active=false;
      }
    }
    this.effects.compute(frames);
    const el=this.effects.output(0), er=this.effects.output(1);
    let peak=0, invalid=0;
    for (let i=0;i<frames;i++) {
      left[i] = Number.isFinite(el[i]) ? el[i] : 0;
      right[i] = Number.isFinite(er[i]) ? er[i] : 0;
      if (!Number.isFinite(el[i]) || !Number.isFinite(er[i])) invalid++;
      peak = Math.max(peak,Math.abs(left[i]),Math.abs(right[i]));
    }
    return {peak,invalid,active:this.voices.filter(v=>v.active).length};
  }
}
