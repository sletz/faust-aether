// Standalone sequenced audition. Starts immediately without MIDI or audio input.
// The full polyphonic demonstration is rendered by `make demo`.
declare name "Aether Audition";
core = library("aether.dsp");
process = core.voice(110,0.85,core.seqGate) : core.effect;
