// Named by FAUST_CUSTOM_EDITOR (see CMakeLists.txt): the Faust JUCE architecture
// includes this header in the faust2juce processor and uses it instead of its
// generic interface. It owns a FaustEditorBridge, calls prepare/processMidi/
// processAudio on it, and opens the editor through createFaustEditor().
#pragma once

#include "AetherEditor.h"

using FaustEditorBridge = AetherBridge;

inline juce::AudioProcessorEditor* createFaustEditor (juce::AudioProcessor& processor, FaustEditorBridge& bridge)
{
    return new AetherEditor (processor, bridge);
}
