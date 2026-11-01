// Standalone stereo insert (two inputs, two outputs) reusing exactly the
// instrument effect bus. Test or host the effects on their own while keeping
// their definition in aether.dsp as the single source of truth.
declare name "Aether Effects";
core = library("aether.dsp");
process = core.effect;
