// State shared between the faust2juce processor and the custom editor.
// The processor owns one bridge: the audio thread feeds it (scope, peak, load,
// voice count) and drains the on-screen keyboard; the editor only reads or
// posts requests, so closing and reopening the window loses nothing.
#pragma once

#include "JuceLibraryCode/JuceHeader.h"

#include <array>
#include <atomic>
#include <functional>

struct AetherBridge
{
    static constexpr int scopeSize = 4096;

    // Notes played with the mouse, the computer keyboard or "Play a chord".
    // It also tracks host MIDI, so incoming notes light the keys.
    juce::MidiKeyboardState keyboard;
    juce::AudioProcessLoadMeasurer load;

    // Installed by the patched processor: counts mydsp_poly voices still
    // sounding, including those in their release.
    std::function<int()> countVoices;
    int numVoices = 0;

    std::atomic<int> activeVoices { 0 };
    std::atomic<float> peak { 0.0f };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<bool> panicRequested { false };
    // Editor state that must survive the editor window being closed.
    // No preset is selected until the user picks one: the DSP starts from the
    // defaults declared in aether.dsp.
    std::atomic<int> selectedPreset { -1 };
    std::atomic<int> selectedTab { 0 };

    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> writePosition { 0 };

    void prepare (double rate, int blockSize)
    {
        sampleRate = rate;
        load.reset (rate, blockSize);
    }

    // Called before the Faust MIDI handler decodes the block.
    void processMidi (juce::MidiBuffer& midi, int numSamples)
    {
        // CC 120 mutes voices at once, whatever their release time.
        if (panicRequested.exchange (false))
            for (int channel = 1; channel <= 16; ++channel)
                midi.addEvent (juce::MidiMessage::allSoundOff (channel), 0);
        keyboard.processNextMidiBuffer (midi, 0, numSamples, true);
    }

    // Called after the DSP has written the block: mono scope feed and peak.
    template <typename FloatType>
    void capture (const juce::AudioBuffer<FloatType>& buffer)
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        if (channels == 0)
            return;

        int position = writePosition.load (std::memory_order_relaxed);
        float blockPeak = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < channels; ++c)
                mono += float (buffer.getSample (c, i));
            mono /= float (channels);
            blockPeak = juce::jmax (blockPeak, std::abs (mono));
            scope[size_t (position)].store (mono, std::memory_order_relaxed);
            position = (position + 1) % scopeSize;
        }
        writePosition.store (position, std::memory_order_release);

        // Keep the maximum since the editor last read it.
        float previous = peak.load();
        while (blockPeak > previous && ! peak.compare_exchange_weak (previous, blockPeak)) {}

        if (countVoices)
            activeVoices = countVoices();
    }
};
