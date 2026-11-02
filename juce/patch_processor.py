#!/usr/bin/env python3
"""Connect the faust2juce processor to the Aether editor.

faust2juce generates FaustPluginProcessor.cpp from the stock juce-plugin.cpp
architecture. This script applies a few anchored edits instead of forking that
architecture: each anchor must occur the expected number of times, so a change
in a future Faust release fails loudly rather than producing a silent mismatch.
"""
import sys
from pathlib import Path

# The editor reads parameters back and recalls presets exactly, which needs the
# JuceParameterUI.h fixes on Faust master-dev (2895e16d2 and 678845a63): host
# parameters report the live zone, and floats keep the Faust step instead of a
# 0.01 grid. Each marker is a line those commits add to the export.
REQUIRED_FAUST_FIXES = [
    'virtual float getValue() const override',
    'juce::NormalisableRange<float>(float(min), float(max), float(step))',
]

PATCHES = [
    # The editor and its bridge only need JUCE, so they come right after it.
    ('#include "JuceLibraryCode/JuceHeader.h"\n',
     '#include "JuceLibraryCode/JuceHeader.h"\n#include "AetherEditor.h"\n', 1),
    # One bridge per processor (the PLUGIN_MAGIC variant gets one too, unused).
    ('        JuceParameterUI fParameterUI;\n',
     '        JuceParameterUI fParameterUI;\n        AetherBridge fBridge;\n', 2),
    # Let the bridge count the voices of the polyphonic instrument.
    # faust2juce compiles with -scn base_dsp, hence the variable name.
    ('\n    base_dsp* base_dsp = new mydsp_poly(new mydsp(), nvoices, true, group);\n',
     '\n    base_dsp* base_dsp = new mydsp_poly(new mydsp(), nvoices, true, group);\n'
     '    fBridge.numVoices = nvoices;\n'
     '    fBridge.countVoices = [poly = static_cast<mydsp_poly*>(base_dsp)] {\n'
     '        int count = 0;\n'
     '        for (auto* voice : poly->fVoiceTable) count += (voice->fCurNote != kFreeVoice);\n'
     '        return count;\n'
     '    };\n', 1),
    ('    fDSP->instanceConstants(int(sampleRate));\n    fDSP->instanceClear();\n',
     '    fDSP->instanceConstants(int(sampleRate));\n    fDSP->instanceClear();\n'
     '    fBridge.prepare(sampleRate, samplesPerBlock);\n', 1),
    # Merge on-screen notes before Faust decodes MIDI, measure the whole block.
    ('    fMIDIHandler->decodeBuffer(midiMessages);\n',
     '    juce::AudioProcessLoadMeasurer::ScopedTimer loadTimer(fBridge.load, buffer.getNumSamples());\n'
     '    fBridge.processMidi(midiMessages, buffer.getNumSamples());\n'
     '    fMIDIHandler->decodeBuffer(midiMessages);\n', 1),
    ('    fHostAdapter.compute(fDSP.get(), buffer, 0, buffer.getNumSamples(), '
     'getTotalNumInputChannels(), getTotalNumOutputChannels());\n',
     '    fHostAdapter.compute(fDSP.get(), buffer, 0, buffer.getNumSamples(), '
     'getTotalNumInputChannels(), getTotalNumOutputChannels());\n'
     '    fBridge.capture(buffer);\n', 1),
    ('    return new FaustPlugInAudioProcessorEditor (*this);\n',
     '    return new AetherEditor (*this, fBridge);\n', 1),
    # Delay and an 8 s reverb keep ringing after the last note.
    ('double FaustPlugInAudioProcessor::getTailLengthSeconds() const\n{\n    return 0.0;\n',
     'double FaustPlugInAudioProcessor::getTailLengthSeconds() const\n{\n    return 12.0;\n', 1),
]


def main(path):
    source = Path(path).read_text()
    if '#include "AetherEditor.h"' in source:
        sys.exit(f'{path} is already patched; regenerate it with faust2juce first.')
    for marker in REQUIRED_FAUST_FIXES:
        if marker not in source:
            sys.exit(f'{path} comes from a Faust without the JuceParameterUI.h fixes '
                     f'(missing: {marker}). Install Faust from master-dev.')
    for old, new, expected in PATCHES:
        found = source.count(old)
        if found != expected:
            sys.exit(f'{path}: expected {expected} occurrence(s) of:\n{old}\nfound {found}.')
        source = source.replace(old, new)
    Path(path).write_text(source)
    print(f'Patched {path} ({len(PATCHES)} edits).')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: patch_processor.py FaustPluginProcessor.cpp')
    main(sys.argv[1])
