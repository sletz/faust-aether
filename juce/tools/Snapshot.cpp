// Offscreen check of the plugin: plays a chord through the processor, reports
// the output level, then renders the editor once per tab (and once after
// selecting a preset) to PNG files. Usage: AetherSnapshot <output folder>
#include "JuceLibraryCode/JuceHeader.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
template <typename Type>
Type* findChild (juce::Component& root, const juce::String& name)
{
    for (auto* child : root.getChildren())
    {
        if (auto* typed = dynamic_cast<Type*> (child); typed != nullptr && typed->getName() == name)
            return typed;
        if (auto* found = findChild<Type> (*child, name))
            return found;
    }
    return nullptr;
}

float render (juce::AudioProcessor& processor, juce::MidiBuffer midi, int blocks)
{
    juce::AudioBuffer<float> buffer (2, 512);
    float peak = 0.0f;
    for (int i = 0; i < blocks; ++i)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();
        peak = juce::jmax (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
    }
    return peak;
}

void pump (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

void save (juce::Component& editor, const juce::File& file)
{
    auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << "wrote " << file.getFullPathName() << "\n";
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    folder.createDirectory();

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->setPlayConfigDetails (0, 2, 48000.0, 512);
    processor->prepareToPlay (48000.0, 512);

    const float silence = render (*processor, {}, 20);
    juce::MidiBuffer chord;
    for (int note : { 48, 55, 60, 63 })
        chord.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
    const float playing = render (*processor, chord, 120);
    std::cout << "peak before notes " << silence << ", while playing " << playing << "\n";

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    editor->setSize (1180, 890);
    pump (200);
    render (*processor, {}, 8); // fresh samples for the scope
    pump (200);

    for (const auto* tab : { "Engines", "Filter & ADSR", "Modulation", "Effects", "Sequencer" })
    {
        if (auto* button = findChild<juce::Button> (*editor, tab))
            button->onClick();
        pump (150);
        save (*editor, folder.getChildFile ("aether-" + juce::String (tab).toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz") + ".png"));
    }

    if (auto* card = findChild<juce::Button> (*editor, "Acid"))
        card->onClick();
    if (auto* button = findChild<juce::Button> (*editor, "Engines"))
        button->onClick();
    render (*processor, chord, 40);
    pump (200);
    save (*editor, folder.getChildFile ("aether-preset-acid.png"));

    editor = nullptr;
    processor->releaseResources();
    return playing > 1.0e-3f && silence < 1.0e-6f ? 0 : 1;
}
