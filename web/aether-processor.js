// Adapter between the main thread (DOM/MIDI) and the audio thread.
// Messages only carry notes/controls; DSP computation happens here.
import { AetherEngine } from './aether-engine.js';
class AetherProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    try {
      // Compiled WASM modules arrive through processorOptions; each voice then
      // receives a separate instance. AudioWorklet supplies sampleRate.
      const o=options.processorOptions;
      this.engine = new AetherEngine(o.voice,o.effects,sampleRate,o.polyphony,o.controls);
      this.frames=0; this.busy=0; this.peak=0; this.invalid=0;
      // Protocol shared with app.js: noteOn, noteOff, control, polyphony, panic.
      // Initialization errors are sent back for display in the interface.
      this.port.onmessage = ({data:m}) => {
        if (m.type==='noteOn') this.engine.noteOn(m.key,m.velocity);
        if (m.type==='noteOff') this.engine.noteOff(m.key);
        if (m.type==='control') this.engine.set(m.key,m.value);
        if (m.type==='polyphony') this.engine.setPolyphony(m.count);
        if (m.type==='panic') this.engine.panic();
      };
      this.port.postMessage({type:'ready'});
    } catch(e) { this.port.postMessage({type:'error',message:e.message}); throw e; }
  }
  // Web Audio supplies output buffers for the quantum; this synthesizer
  // consumes no input. Returning true keeps the processor alive, particularly
  // while only reverb tails are still sounding.
  process(inputs,outputs) {
    const out=outputs[0];
    if (!out?.[0] || !out?.[1]) return true;
    const t=globalThis.performance?.now?.() ?? Date.now();
    const stats=this.engine.compute(out[0].length,out[0],out[1]);
    this.busy+=(globalThis.performance?.now?.() ?? Date.now())-t;
    this.frames+=out[0].length; this.peak=Math.max(this.peak,stats.peak); this.invalid+=stats.invalid;
    // Publish counters roughly four times a second rather than every block.
    // load = computation time / produced audio duration, as a percentage.
    // Counters accumulate over this window and are then reset.
    if(this.frames>=sampleRate*0.25) {
      this.port.postMessage({type:'stats',active:stats.active,peak:this.peak,
        invalid:this.invalid,load:this.busy/(this.frames/sampleRate*1000)*100,
        keys:this.engine.voices.filter(v=>v.active&&v.held).map(v=>v.key)});
      this.frames=0; this.busy=0; this.peak=0; this.invalid=0;
    }
    return true;
  }
}
registerProcessor('aether',AetherProcessor);
