// AETHER — hybrid polyphonic synthesizer and shared stereo effects.
// Voice signal path: engines → coloration → filter → amplitude/panning.
// The host instantiates process for each note, sums the voices, then runs
// effect once. This separation preserves effect tails after voice release
// and avoids duplicating their large buffers.
declare name "Aether";
declare author "Faust example project";
declare version "1.0.0";
declare license "MIT";
declare options "[midi:on][nvoices:12]";
// Tail reported to plugin hosts (faust2juce): ping-pong repeats and a reverb of
// up to 8 s keep sounding after the last note.
declare tail_sec "12";
import("stdfaust.lib");

// Utilities: clamping, semitones to frequency ratio, and smoothing.
// si.smoo smooths changes at audio rate to reduce parameter-change clicks;
// ADSR timing is handled separately by en.adsr.
clip(lo,hi,x) = min(hi,max(lo,x));
semi(x) = pow(2,x/12);
sm(x) = x : si.smoo;
// The ceiling follows the actual host sample rate.
// It leaves headroom below Nyquist; it does not remove aliasing subsequently
// introduced by FM, wavefolding, or saturation without oversampling.
safeHz(x) = clip(12,0.43*ma.SR,x);

// Names recognized by polyphonic hosts and faustprobe: freq in hertz,
// gain for normalized velocity, gate for note hold and subsequent release.
// UI groups also define the control addresses discovered by JavaScript.
freq = hslider("freq[unit:Hz][scale:log]",110,20,20000,0.01);
gain = hslider("gain",0.7,0,1,0.001);
gate = button("gate");
voiceUI(x) = vgroup("[0] Voice",x);
oscUI(x) = voiceUI(vgroup("[1] Engines",x));
envUI(x) = voiceUI(vgroup("[2] Envelopes",x));
filterUI(x) = voiceUI(vgroup("[3] Filter",x));
modUI(x) = voiceUI(vgroup("[4] Modulation",x));
seqUI(x) = voiceUI(vgroup("[5] Sequencer",x));
fxUI(x) = vgroup("[1] Effects",x);

// Engine mix and basic tone. Unison uses seven detuned oscillators, each
// interpolating between the library saw, pulse/PWM, and triangle waveforms
// with attenuated aliasing. The engines keep computing at zero volume.
vaLevel = oscUI(hslider("va_level",0.72,0,1,0.001)) : si.smoo;
fmLevel = oscUI(hslider("fm_level",0.20,0,1,0.001)) : si.smoo;
addLevel = oscUI(hslider("add_level",0.18,0,1,0.001)) : si.smoo;
subLevel = oscUI(hslider("sub_level",0.25,0,1,0.001)) : si.smoo;
noiseLevel = oscUI(hslider("noise_level",0.025,0,0.5,0.001)) : si.smoo;
detune = oscUI(hslider("detune_cents",12,0,45,0.1)) : si.smoo;
morph = oscUI(hslider("morph",0.18,0,2,0.001)) : si.smoo;
pulse = oscUI(hslider("pulse_width",0.42,0.08,0.92,0.001)) : si.smoo;
fmIndex = oscUI(hslider("fm_index",1.5,0,12,0.01)) : si.smoo;
fmRatio = oscUI(hslider("fm_ratio",2,0.25,8,0.01)) : si.smoo;
fmAlgo = oscUI(hslider("fm_parallel",0.35,0,1,0.001)) : si.smoo;
brightness = oscUI(hslider("brightness",0.5,0,1,0.001)) : si.smoo;
inharm = oscUI(hslider("inharmonicity",0.015,0,0.35,0.001)) : si.smoo;
ring = oscUI(hslider("ring_mod",0,0,1,0.001)) : si.smoo;
fold = oscUI(hslider("fold",0.08,0,1,0.001)) : si.smoo;
drive = oscUI(hslider("drive",1.4,1,8,0.01)) : si.smoo;

// Two complete ADSRs: amplitude and filter opening. The third envelope,
// defined inside voice with zero sustain, controls FM among other things.
// Times are in seconds; sustain values are levels in the 0..1 range.
attack = envUI(hslider("amp_attack[unit:s]",0.025,0.001,5,0.001));
decay = envUI(hslider("amp_decay[unit:s]",0.45,0.005,5,0.001));
sustain = envUI(hslider("amp_sustain",0.65,0,1,0.001));
release = envUI(hslider("amp_release[unit:s]",0.9,0.005,8,0.001));
fAttack = envUI(hslider("filter_attack[unit:s]",0.012,0.001,4,0.001));
fDecay = envUI(hslider("filter_decay[unit:s]",0.65,0.005,6,0.001));
fSustain = envUI(hslider("filter_sustain",0.18,0,1,0.001));
fRelease = envUI(hslider("filter_release[unit:s]",0.7,0.005,8,0.001));
mDecay = envUI(hslider("mod_decay[unit:s]",1.2,0.005,8,0.001));
// The filter combines key tracking, envelope, velocity, and matrix modulation
// in the octave domain, so a given depth has a comparable musical effect
// at different cutoff frequencies.
cutoff = filterUI(hslider("cutoff[unit:Hz][scale:log]",1800,30,18000,1)) : si.smoo;
resonance = filterUI(hslider("resonance",0.35,0,0.92,0.001)) : si.smoo;
fEnvAmount = filterUI(hslider("filter_env_octaves",3,0,7,0.01)) : si.smoo;
keytrack = filterUI(hslider("keytrack",0.5,0,1,0.001)) : si.smoo;
fType = filterUI(hslider("filter_type[tooltip:0=ladder 1=bandpass 2=highpass]",0,0,2,0.001)) : si.smoo;

// Slow sources and performance controls available to modulation destinations.
// Each voice instantiates its own LFOs and envelopes.
lfo1Rate = modUI(hslider("lfo1_hz",0.31,0.01,25,0.01)) : si.smoo;
lfo2Rate = modUI(hslider("lfo2_hz",4.7,0.01,30,0.01)) : si.smoo;
macro1 = modUI(hslider("macro1",0,0,1,0.001)) : si.smoo;
macro2 = modUI(hslider("macro2",0,0,1,0.001)) : si.smoo;
velocityCutoff = modUI(hslider("velocity_cutoff_octaves",1.2,0,4,0.01)) : si.smoo;
spread = modUI(hslider("stereo_spread",0.72,0,1,0.001)) : si.smoo;

// Eight independent routes: source × depth, summed by destination.
// A negative depth reverses the movement; index 0 disables a source or
// destination.
// Sources: 1=LFO1, 2=LFO2, 3=filter ADSR, 4=percussive envelope,
//          5=bipolar velocity, 6=transposition/72, 7=macro1, 8=macro2.
// Destinations: 1=pitch, 2=cutoff, 3=FM index, 4=waveform, 5=pan, 6=amplitude.
// Defaults: LFO1 → cutoff and a subtle LFO2 → pitch.
slotUI(i,x) = modUI(vgroup("Route %i",x));
slotSrc(i) = slotUI(i,hslider("source",select2(i<2,0,i+1),0,8,1));
slotDst(i) = slotUI(i,hslider("destination",select2(i==0,select2(i==1,0,1),2),0,6,1));
slotDepth(i) = slotUI(i,hslider("depth",select2(i==0,select2(i==1,0,0.009),0.12),-1,1,0.001)) : si.smoo;

// Four steps per beat, sixteen steps per cycle. A slow ramp selects the step;
// a ramp at the step rate controls its gate length.
// sequence_octaves multiplies intervals rather than adding an octave.
bpm = seqUI(hslider("bpm",112,30,240,0.1));
seqOn = seqUI(checkbox("sequencer"));
seqOct = seqUI(hslider("sequence_octaves",1,0,3,1));
seqLength = seqUI(hslider("step_gate",0.7,0.1,0.98,0.001));
seqStep = int(os.lf_sawpos(bpm/60*4/16)*16) : clip(0,15);
seqPhase = os.lf_sawpos(bpm/60*4);
stepNote(i) = seqUI(vgroup("Steps",hslider("note_%i",pattern(i),-24,24,1)));
// Write the initial pattern as constants: widget defaults must be known
// at compile time, before any audio signal is evaluated.
pattern(0) = 0;
pattern(1) = 7;
pattern(2) = 12;
pattern(3) = 3;
pattern(4) = 0;
pattern(5) = 10;
pattern(6) = 7;
pattern(7) = 15;
pattern(8) = 0;
pattern(9) = 7;
pattern(10) = 19;
pattern(11) = 12;
pattern(12) = 3;
pattern(13) = 10;
pattern(14) = 7;
pattern(15) = -5;
seqPitch = ba.selectn(16,seqStep,par(i,16,stepNote(i)))*seqOct;
seqGate = seqPhase < seqLength;
seqValue = seqPitch/72;

// Triangular waveform crossfade: m=0 saw, m=1 pulse, m=2 triangle.
// Between landmarks, the two weights sum to one; pw sets the pulse duty cycle,
// which is also gently animated by the second LFO.
morphOsc(f,m,pw) = (1-min(1,m))*os.sawtooth(f)
    + (1-abs(m-1))*os.pulsetrain(f,pw)
    + max(0,m-1)*os.triangle(f);

// FM-style synthesis implemented as phase modulation (os.oscp, in radians).
// The serial branch chains op4 → op3 → op2 → carrier; the parallel branch
// sums two modulated carriers. fmAlgo interpolates the two results.
// The envelope reduces the index over the note, darkening its timbre.
fmEngine(f,idx,env) = (1-fmAlgo)*serial + fmAlgo*parallel with {
    op4 = os.osc(safeHz(f*fmRatio*1.003));
    op3 = os.oscp(safeHz(f*fmRatio),idx*0.25*op4);
    op2 = os.oscp(safeHz(f*0.5*fmRatio),idx*0.35*env*op3);
    serial = os.oscp(f,idx*env*op2);
    parallel = 0.5*(os.oscp(f,idx*env*op4)+os.oscp(safeHz(f*1.005),idx*env*op3));
};

// Twelve partials: brightness sets spectral tilt; inharm stretches frequencies,
// especially the upper partials. Their amplitudes fade between 0.40 and 0.46
// times SR, before reaching Nyquist.
addEngine(f) = sum(i,12,partial(i))/3.2 with {
    partial(i) = os.osc(safeHz(hz)) * pow(i+1,2*brightness-2.8)
        * clip(0,1,(0.46*ma.SR-hz)/(0.06*ma.SR)) with {
        hz = f*(i+1)*sqrt(1+inharm*(i+1)*(i+1));
    };
};

// A voice directly produces two channels and consumes no audio input.
// The host only supplies its note frequency, velocity, and gate.
voice(noteFreq,velocity,noteGate) = outL,outR with {
    // The sequencer chops a held note. Note-off always lowers the gate and
    // starts the releases, even in the middle of an active step.
    g = noteGate*select2(seqOn,1,seqGate);
    amp = en.adsr(attack,decay,sustain,release,g);
    fe = en.adsr(fAttack,fDecay,fSustain,fRelease,g);
    me = en.adsr(0.003,mDecay,0,0.2,g);
    l1 = os.osc(lfo1Rate);
    l2 = os.triangle(lfo2Rate);
    // Select sources, then sum by destination. Pitch changes are in semitones
    // (12 × modulation); filter changes are in octaves.
    source(s) = ba.selectn(9,int(s),0,l1,l2,fe,me,2*velocity-1,seqValue,macro1,macro2);
    mod(d) = sum(i,8,(slotDst(i)==d)*slotDepth(i)*source(slotSrc(i)));
    f = safeHz(noteFreq*semi(seqOn*seqPitch+12*mod(1)));
    mm = clip(0,2,morph+mod(4));
    pw = clip(0.08,0.92,pulse+0.08*l2);
    idx = clip(0,16,fmIndex+8*mod(3));
    p = clip(0.02,0.98,0.5+0.48*mod(5));
    // Spread detuning symmetrically around the note and panning around center.
    // Square-root weights provide equal-power panning. Dividing by five leaves
    // headroom when summing the seven oscillators.
    unison(ch) = sum(i,7,morphOsc(safeHz(f*semi((i-3)*detune/300)),mm,pw)
                  *sqrt(clip(0,1,select2(ch,1-pos(i),pos(i)))))/5 with {
        pos(i) = 0.5+spread*(i-3)/6;
    };
    // FM, additive, sub-octave, and noise form a centered mono core.
    // Unison supplies left/right differences before the global effects.
    body = fmLevel*fmEngine(f,idx,me) + addLevel*addEngine(f)
        + subLevel*os.osc(f*0.5) + noiseLevel*no.pink_noise;
    tone(ch) = vaLevel*unison(ch)+body*0.7071;
    // Nonlinearities before the filter: crossfade into the wavefolder, saturate,
    // then multiply by a sine wave for ring modulation.
    color(x) = ma.tanh(drive*((1-fold)*x+fold*(ef.wavefold(0.65,x*3))))
        *(1-ring+ring*os.osc(safeHz(f*1.5)));
    // Key tracking uses 110 Hz as its reference. The exponential converts the
    // sum of octave offsets into frequency, which safeHz then clamps.
    // The ladder expects normalized logarithmic frequency; the resonant filters
    // expect hertz. fType interpolates their outputs rather than coefficients,
    // allowing a gradual transition between filter colors.
    fc = safeHz(cutoff*pow(2,keytrack*ma.log2(max(20,noteFreq)/110)
        + fEnvAmount*fe + 4*mod(2) + velocityCutoff*(velocity-0.5)));
    filter(x) = (1-min(1,fType))*ve.moogLadder(clip(0,1,log10(fc/20)/3),0.71+resonance*18,x)
        + (1-abs(fType-1))*fi.resonbp(fc,0.6+resonance*7,1,x)
        + max(0,fType-1)*fi.resonhp(fc,0.6+resonance*4,1,x);
    // The gate does not cut the signal directly: the ADSR retains its release.
    // DC rejection reduces offsets from the nonlinearities before panning.
    amplitude = 0.3*velocity*amp*clip(0,2,1+mod(6));
    outL = tone(0) : color : filter : fi.dcblocker : *(amplitude*sqrt(2*(1-p)));
    outR = tone(1) : color : filter : fi.dcblocker : *(amplitude*sqrt(2*p));
};

// Global bus after the polyphonic sum. At zero mix, each stage passes its
// dry signal but keeps updating its state: reopening the mix reveals the
// already-fed delays/reverb without an abrupt reset.
chorusMix = fxUI(vgroup("[0] Ensemble",hslider("chorus_mix",0.28,0,1,0.001))) : si.smoo;
chorusRate = fxUI(vgroup("[0] Ensemble",hslider("chorus_rate",0.23,0.03,3,0.01))) : si.smoo;
chorusDepth = fxUI(vgroup("[0] Ensemble",hslider("chorus_depth_ms",4.5,0,9,0.01))) : si.smoo;
phaserMix = fxUI(vgroup("[1] Phaser",hslider("phaser_mix",0.12,0,1,0.001))) : si.smoo;
phaserRate = fxUI(vgroup("[1] Phaser",hslider("phaser_rate",0.12,0.02,5,0.01))) : si.smoo;
delayMix = fxUI(vgroup("[2] Ping pong",hslider("delay_mix",0.22,0,0.8,0.001))) : si.smoo;
delayFB = fxUI(vgroup("[2] Ping pong",hslider("delay_feedback",0.46,0,0.88,0.001))) : si.smoo;
delayBeats = fxUI(vgroup("[2] Ping pong",hslider("delay_beats",0.75,0.125,2,0.125))) : si.smoo;
delayTone = fxUI(vgroup("[2] Ping pong",hslider("delay_tone[unit:Hz]",4200,300,12000,1))) : si.smoo;
reverbMix = fxUI(vgroup("[3] Space",hslider("reverb_mix",0.27,0,0.9,0.001))) : si.smoo;
reverbTime = fxUI(vgroup("[3] Space",hslider("reverb_seconds",3.8,1,8,0.01))) : si.smoo : max(1);
predelay = fxUI(vgroup("[3] Space",hslider("predelay_ms",30,20,100,1)));
bass = fxUI(vgroup("[4] Master",hslider("bass_db",0,-12,9,0.1))) : si.smoo;
air = fxUI(vgroup("[4] Master",hslider("air_db",1,-12,9,0.1))) : si.smoo;
master = fxUI(vgroup("[4] Master",hslider("master_db",-3,-48,6,0.1))) : ba.db2linear : si.smoo;
width = fxUI(vgroup("[4] Master",hslider("width",1.15,0,1.6,0.01))) : si.smoo;

// Chorus: three fractional reads of the same signal per channel, with
// different rates and phases. Averaging the taps stabilizes their level.
chorusChannel(ch,x) = (1-chorusMix)*x+chorusMix*sum(i,3,de.fdelay(16384,dt(i),x))/3 with {
    // Storage capacity is a fixed integer; read positions are clamped at audio
    // rate rather than deriving storage size from a floating-point calculation.
    dt(i) = clip(1,16380,ma.SR*0.001*(16+4*i+chorusDepth
        *os.oscp(chorusRate*(1+0.11*i),ma.PI*(ch+0.6*i))));
};
chorus = chorusChannel(0),chorusChannel(1);
// Compute the phaser once in stereo; _,! and !,_ select its left and right
// outputs respectively for the dry/wet crossfade.
phaser(l,r) = (1-phaserMix)*l+phaserMix*pl,(1-phaserMix)*r+phaserMix*pr with {
    wet = pf.phaser2_stereo(4,80,100,1.6,min(5000,0.35*ma.SR),phaserRate,0.7,0.25,0,l,r);
    pl = wet : _,!; pr = wet : !,_;
};

// Ping-pong: the right feedback feeds the left delay and vice versa.
// The low-pass darkens each repeat, and tanh bounds the recirculating signal.
// Tempo and delayBeats give a duration in samples, clamped within fixed-capacity
// storage. The ~ operator adds one feedback sample; subtract it from the read
// delay to preserve the requested total duration.
pingpong(l,r) = (1-delayMix)*l+delayMix*dl,(1-delayMix)*r+delayMix*dr with {
    delaySamples = clip(1,1048570,ma.SR*60/max(30,bpm)*delayBeats);
    loop(a,b) = (de.fdelay(1048576,delaySamples-1,l+delayFB*b)
                 : fi.lowpass(1,safeHz(delayTone)) : ma.tanh),
               (de.fdelay(1048576,delaySamples-1,r+delayFB*a)
                 : fi.lowpass(1,safeHz(delayTone)) : ma.tanh);
    wet = loop ~ (_, _);
    dl = wet : _,!; dr = wet : !,_;
};
// Shared Zita reverb: bass decays 20% longer than the midrange.
// Storage supports up to 192 kHz; reverbTime stays at least one second,
// including during the initial ramp of its smoothing filter.
space(l,r) = (1-reverbMix)*l+reverbMix*wl,(1-reverbMix)*r+reverbMix*wr with {
    wet = re.zita_rev1_stereo(predelay,200,min(6500,0.35*ma.SR),reverbTime*1.2,reverbTime,192000,l,r);
    wl = wet : _,!; wr = wet : !,_;
};
// Mid/side width: width=0 retains the mono center, width=1 restores stereo,
// and width>1 emphasizes differences. Bass/treble EQ and volume precede
// a final soft clip whose finite output remains within ±0.92.
stereoWidth(l,r) = mid+side*width,mid-side*width with { mid=(l+r)*0.5; side=(l-r)*0.5; };
masterChannel = fi.low_shelf(bass,180) : fi.high_shelf(air,min(7000,0.35*ma.SR))
    : fi.dcblocker : *(master) : ma.tanh : *(0.92);
// Colon chains the stages; comma processes the two channels in parallel.
// Linked stereo compression preserves their relative levels.
effect = chorus : phaser : pingpong : space : stereoWidth
    : co.compressor_stereo(3,-12,0.008,0.18) : (masterChannel,masterChannel);
process = voice(freq,gain,gate);
