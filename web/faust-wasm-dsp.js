// WASM loader adapted from faust-coopiano/web/faust-wasm-dsp.js.
// Exposes the Faust contract (memory, JSON offsets, compute) without browser
// dependencies, for use in both AudioWorklet and Node.

/**
 * Math imports expected in env by generated Faust code.
 * Each function exists as _name (double) and _namef (single precision).
 * JavaScript computes in double; WASM performs float32 conversions.
 * @returns {Object<string, Function>} Functions supplied to WebAssembly.Instance.
 */
export function mathImports() {
  const fmod = (a, b) => a % b;
  // remainder rounds the quotient to the nearest integer, ties to even,
  // unlike fmod (%) and Math.round. round separately reproduces C rounding
  // of half-integers away from zero, including for x<0.
  const remainder = (a, b) => {
    const q = a / b, lo = Math.floor(q), f = q - lo;
    const nearest = f < 0.5 ? lo : f > 0.5 ? lo + 1 : lo + Math.abs(lo % 2);
    return a - nearest * b;
  };
  const round = (x) => x < 0 ? -Math.round(-x) : Math.round(x);
  // Object.is distinguishes -0 from +0; the C predicates return integers.
  const copysign = (x, y) => Math.abs(x) * (Object.is(y, -0) || y < 0 ? -1 : 1);
  const isnan = (x) => (Number.isNaN(x) ? 1 : 0);
  const isinf = (x) => (Number.isFinite(x) || Number.isNaN(x) ? 0 : 1);
  const exp10 = (x) => 10 ** x;
  const env = {};
  const table = {
    sin: Math.sin, cos: Math.cos, exp: Math.exp, exp10, log: Math.log, log10: Math.log10,
    tan: Math.tan, atan: Math.atan, asin: Math.asin, acos: Math.acos, round,
    pow: Math.pow, atan2: Math.atan2, fmod, remainder, isnan, isinf, copysign,
    acosh: Math.acosh, asinh: Math.asinh, atanh: Math.atanh, cosh: Math.cosh,
    sinh: Math.sinh, tanh: Math.tanh,
  };
  for (const [name, f] of Object.entries(table)) {
    env["_" + name] = f;
    env["_" + name + "f"] = f;
  }
  return env;
}
/**
 * Flatten the Faust JSON groups into leaves: widgets and bargraphs.
 * @param {Array<Object>} ui The ui tree or a subgroup's items.
 * @param {Array<Object>} out Depth-first traversal accumulator.
 * @returns {Array<Object>} Leaves in their original order.
 */
export function uiItems(ui, out = []) {
  for (const item of ui || []) {
    if (Array.isArray(item.items)) uiItems(item.items, out);
    else out.push(item);
  }
  return out;
}
/**
 * Remove the DSP name and the optional legacy settings group.
 * Example: /Aether/Voice/Engines/drive becomes Voice/Engines/drive.
 * Retain internal paths to distinguish the eight modulation routes.
 * @param {string} address Full address discovered in the Faust JSON.
 * @returns {string} Stable key shared by the interface and engine.
 */
export function controlKey(address) {
  const parts = address.split("/").filter(Boolean).slice(1);
  if (parts[0] === "settings") parts.shift();
  return parts.join("/");
}
// Align pointer tables to 4 bytes and samples to 4 or 8 bytes.
const align = (value, alignment) => Math.ceil(value / alignment) * alignment;
/**
 * One Faust instance and its private memory. The DSP starts at address 0;
 * json.size reserves its full state, followed by two 32-bit pointer tables
 * (inputs, outputs), then aligned audio buffers for each channel.
 * Widget index values are byte offsets into this state.
 */
export class FaustWasmDsp {
  /**
   * @param {{module: WebAssembly.Module, json: Object}} compiled WASM and metadata.
   * @param {number} sampleRate Sample rate passed to init.
   * @param {number} blockSize Maximum buffer capacity in frames.
   */
  constructor(compiled, sampleRate, blockSize) {
    const { module, json } = compiled;
    this.json = json;
    this.blockSize = blockSize;
    const env = mathImports();
    // Depending on the backend, memory is exported or imported from env.
    // Imported memory starts with one 64 KiB page and is grown below.
    const importsMemory = WebAssembly.Module.imports(module).some((i) => i.module === "env" && i.name === "memory");
    if (importsMemory) env.memory = new WebAssembly.Memory({ initial: 1 });
    this.exports = new WebAssembly.Instance(module, { env }).exports;
    const memory = this.exports.memory || env.memory;
    // Control and buffer formats must match compilation; do not infer precision
    // from the JavaScript engine or Web Audio.
    this.double = /(^|\s)-double(\s|$)/.test(json.compile_options || "");
    const bytes = this.double ? 8 : 4;
    this.inputs = this.exports.getNumInputs(0);
    this.outputs = this.exports.getNumOutputs(0);
    // Place tables and buffers after the DSP state so compute cannot overwrite
    // controls or internal delay lines.
    this.inputsPtr = align(Number(json.size), 4);
    this.outputsPtr = this.inputsPtr + this.inputs * 4;
    let cursor = align(this.outputsPtr + this.outputs * 4, bytes);
    const channels = [];
    for (let c = 0; c < this.inputs + this.outputs; c += 1) {
      channels.push(cursor);
      cursor += blockSize * bytes;
    }
    // memory.grow invalidates old ArrayBuffer views: create all typed views
    // after growth, then reuse them in compute.
    if (cursor > memory.buffer.byteLength) {
      memory.grow(Math.ceil((cursor - memory.buffer.byteLength) / 65536));
    }
    const u32 = new Uint32Array(memory.buffer);
    // The two tables are contiguous; each entry holds a WASM address rather
    // than a JS array index. Pointers always remain 32-bit.
    channels.forEach((ptr, c) => {
      u32[(this.inputsPtr >> 2) + c] = ptr;
    });
    const Heap = this.double ? Float64Array : Float32Array;
    this.channelViews = channels.map((ptr) => new Heap(memory.buffer, ptr, blockSize));
    // Initialize the instance at address zero, including its oscillator tables.
    // Subsequent setParamValue calls use the offsets from the accompanying JSON.
    this.exports.init(0, sampleRate);
    this.controls = new Map();
    for (const item of uiItems(json.ui)) {
      if (item.address !== undefined && Number.isInteger(item.index)) {
        this.controls.set(controlKey(item.address), item.index);
      }
    }
  }
  /**
   * Write a recognized control. Ignoring missing keys allows broadcasting
   * one patch to the voice and bus without knowing which DSP owns each control.
   * @param {string} key Address without the DSP name.
   * @param {number} value Native value expected by the control (Hz, seconds...).
   */
  set(key, value) {
    const index = this.controls.get(key);
    if (index !== undefined) this.exports.setParamValue(0, index, value);
  }
  /** @returns {number|undefined} Control value, or undefined if absent. */
  get(key) {
    const index = this.controls.get(key);
    return index === undefined ? undefined : this.exports.getParamValue(0, index);
  }
  /** @returns {Float32Array|Float64Array} Input buffer c, to fill before compute. */
  input(c) {
    return this.channelViews[c];
  }
  /** @returns {Float32Array|Float64Array} Output buffer c after compute. */
  output(c) {
    return this.channelViews[this.inputs + c];
  }
  // Reuse preallocated tables and buffers; frames must not exceed blockSize.
  // The first argument of exported functions is the DSP pointer.
  compute(frames) {
    this.exports.compute(0, frames, this.inputsPtr, this.outputsPtr);
  }
}
