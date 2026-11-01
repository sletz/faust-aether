// Standalone audition: the sequencer gate retriggers a voice without MIDI
// or audio input. Pitch changes when sequencer is enabled.
// Apply the effect bus explicitly because no polyphonic host extracts effect
// in this version. make demo renders the polyphonic demonstration.
declare name "Aether Audition";
core = library("aether.dsp");
process = core.voice(110,0.85,core.seqGate) : core.effect;
