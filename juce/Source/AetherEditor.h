// Custom editor for the faust2juce export of Aether, modelled on the web
// interface: preset library, live scope and telemetry, tabbed controls, and a
// playable keyboard. It talks to the DSP only through the host parameters that
// JuceParameterUI creates, plus the AetherBridge owned by the processor.
#pragma once

#include "AetherBridge.h"

#include <map>
#include <memory>
#include <vector>

namespace aether
{
struct Param;
class Palette;
class ContentView;
}

class AetherEditor : public juce::AudioProcessorEditor
{
public:
    AetherEditor (juce::AudioProcessor&, AetherBridge&);
    ~AetherEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::unique_ptr<aether::Palette> lookAndFeel;
    std::unique_ptr<aether::ContentView> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherEditor)
};
