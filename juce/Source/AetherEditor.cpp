// Aether editor: a native JUCE take on web/index.html and web/app.js.
// Everything is laid out on a fixed 1180 × 860 design canvas, then scaled as a
// whole, so the window keeps its proportions at any size.
#include "AetherEditor.h"

#include <cmath>

namespace aether
{
constexpr int designWidth = 1180;
constexpr int designHeight = 890;

//==============================================================================
// Colours and fonts from web/style.css.
namespace colour
{
const juce::Colour background { 0xff101615 };
const juce::Colour panel { 0xff18211f };
const juce::Colour line { 0xff2d3a34 };
const juce::Colour text { 0xffe5e8dd };
const juce::Colour muted { 0xff8e9e94 };
const juce::Colour label { 0xffc1cbbf };
const juce::Colour accent { 0xffbbe5c9 };
const juce::Colour amber { 0xffd8ae7e };
const juce::Colour scope { 0xff0b1310 };
const juce::Colour scopeGrid { 0xff1c2a22 };
const juce::Colour trace { 0xffbbdbb2 };
const juce::Colour telemetry { 0xff152018 };
const juce::Colour telemetryLine { 0xff253329 };
const juce::Colour track { 0xff344238 };
const juce::Colour cardActive { 0xff25392c };
const juce::Colour cardBorder { 0xff54725b };
const juce::Colour hover { 0xff1c2822 };
const juce::Colour buttonBorder { 0xff3c4d41 };
const juce::Colour field { 0xff213024 };
const juce::Colour fieldBorder { 0xff3b4e40 };
} // namespace colour

juce::Font sans (float height, bool bold = false, float tracking = 0.0f)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)
                           .withKerningFactor (tracking));
}

juce::Font mono (float height, float tracking = 0.0f)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain)
                           .withKerningFactor (tracking));
}

float textWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

//==============================================================================
// Musical presentation of the Faust controls: names and formatting mirror
// web/app.js; steps and units come from the widget declarations in aether.dsp.
const std::map<juce::String, const char*> displayNames {
    { "va_level", "Analog engine" }, { "fm_level", "FM engine" }, { "add_level", "Additive engine" },
    { "sub_level", "Sub-octave" }, { "noise_level", "Pink noise" }, { "detune_cents", "Detune" },
    { "morph", "Waveform" }, { "pulse_width", "Pulse width" }, { "fm_index", "FM index" },
    { "fm_ratio", "FM ratio" }, { "fm_parallel", "Parallel FM" }, { "brightness", "Brightness" },
    { "inharmonicity", "Inharmonicity" }, { "ring_mod", "Ring modulation" }, { "fold", "Wavefolding" },
    { "drive", "Drive" }, { "amp_attack", "Attack" }, { "amp_decay", "Decay" },
    { "amp_sustain", "Sustain" }, { "amp_release", "Release" }, { "filter_attack", "Attack" },
    { "filter_decay", "Decay" }, { "filter_sustain", "Sustain" }, { "filter_release", "Release" },
    { "mod_decay", "Mod envelope decay" }, { "cutoff", "Cutoff" }, { "resonance", "Resonance" },
    { "filter_env_octaves", "Envelope → filter" }, { "keytrack", "Key tracking" },
    { "filter_type", "Filter type" }, { "lfo1_hz", "LFO 1 / sine" }, { "lfo2_hz", "LFO 2 / triangle" },
    { "macro1", "Macro 1" }, { "macro2", "Macro 2" }, { "velocity_cutoff_octaves", "Velocity → filter" },
    { "stereo_spread", "Stereo unison" }, { "bpm", "Tempo" }, { "sequencer", "Sequencer" },
    { "sequence_octaves", "Interval multiplier" }, { "step_gate", "Step gate length" },
    { "chorus_mix", "Mix" }, { "chorus_rate", "Rate" }, { "chorus_depth_ms", "Depth" },
    { "phaser_mix", "Mix" }, { "phaser_rate", "Rate" }, { "delay_mix", "Mix" },
    { "delay_feedback", "Feedback" }, { "delay_beats", "Time" }, { "delay_tone", "Tone" },
    { "reverb_mix", "Mix" }, { "reverb_seconds", "Decay" }, { "predelay_ms", "Predelay" },
    { "bass_db", "Bass" }, { "air_db", "Air" }, { "master_db", "Volume" }, { "width", "Stereo width" },
};

// Faust steps for the controls that are not 0.001 or 0.01; they set both the
// displayed precision and, for discrete controls, slider snapping.
const std::map<juce::String, float> steps {
    { "cutoff", 1.0f }, { "predelay_ms", 1.0f }, { "delay_tone", 1.0f }, { "bpm", 0.1f },
    { "detune_cents", 0.1f }, { "bass_db", 0.1f }, { "air_db", 0.1f }, { "master_db", 0.1f },
    { "sequence_octaves", 1.0f }, { "delay_beats", 0.125f }, { "source", 1.0f }, { "destination", 1.0f },
};

const std::map<juce::String, const char*> units {
    { "cutoff", "Hz" }, { "delay_tone", "Hz" }, { "lfo1_hz", "Hz" }, { "lfo2_hz", "Hz" },
    { "chorus_rate", "Hz" }, { "phaser_rate", "Hz" }, { "amp_attack", "s" }, { "amp_decay", "s" },
    { "amp_release", "s" }, { "filter_attack", "s" }, { "filter_decay", "s" }, { "filter_release", "s" },
    { "mod_decay", "s" }, { "reverb_seconds", "s" }, { "chorus_depth_ms", "ms" }, { "predelay_ms", "ms" },
    { "bass_db", "dB" }, { "air_db", "dB" }, { "master_db", "dB" }, { "detune_cents", "ct" },
    { "filter_env_octaves", "oct" }, { "velocity_cutoff_octaves", "oct" }, { "bpm", "BPM" },
    { "fm_ratio", "×" }, { "sequence_octaves", "×" }, { "delay_beats", "beats" },
};

// Menu indices must match source() and mod() in aether.dsp.
const juce::StringArray sources { "Disabled", "LFO 1", "LFO 2", "Filter env.", "Mod env.",
                                  "Velocity", "Sequencer", "Macro 1", "Macro 2" };
const juce::StringArray destinations { "Disabled", "Pitch", "Filter", "FM index",
                                       "Waveform", "Pan", "Amplitude" };

//==============================================================================
// One control, addressed by its short key: the widget label, or "Route_i/label"
// for the repeated routing controls. A key can own several host parameters:
// the effect bus reads the voice's bpm for its delay time, and Faust exports
// that do not merge shared controls (older ones, or FAUST_JUCE_FULLPATH_IDS)
// publish bpm once per DSP, so all of them must move together.
struct Param
{
    juce::RangedAudioParameter* parameter = nullptr; // the first one, read back
    std::vector<juce::RangedAudioParameter*> targets;
    juce::String key, leaf;
    float minimum = 0.0f, maximum = 1.0f, defaultValue = 0.0f, step = 0.001f, interval = 0.0f;
    bool isToggle = false, logarithmic = false;
    bool inGesture = false;

    float get() const { return parameter->convertFrom0to1 (parameter->getValue()); }

    float snap (float value) const
    {
        value = juce::jlimit (minimum, maximum, value);
        if (interval > 0.0f)
            value = juce::jlimit (minimum, maximum, minimum + interval * std::round ((value - minimum) / interval));
        return value;
    }

    void set (float value)
    {
        value = snap (value);
        for (auto* target : targets)
        {
            if (! inGesture)
                target->beginChangeGesture();
            target->setValueNotifyingHost (target->convertTo0to1 (value));
            if (! inGesture)
                target->endChangeGesture();
        }
    }

    void beginGesture()
    {
        if (! std::exchange (inGesture, true))
            for (auto* target : targets)
                target->beginChangeGesture();
    }

    void endGesture()
    {
        if (std::exchange (inGesture, false))
            for (auto* target : targets)
                target->endChangeGesture();
    }

    juce::String title() const
    {
        if (leaf.startsWith ("note_"))
            return "Step " + juce::String (leaf.fromFirstOccurrenceOf ("_", false, false).getIntValue() + 1).paddedLeft ('0', 2);
        auto found = displayNames.find (leaf);
        return found != displayNames.end() ? juce::String::fromUTF8 (found->second) : leaf.replace ("_", " ");
    }

    juce::String format (float value) const
    {
        if (isToggle)
            return value > 0.5f ? "ON" : "OFF";
        if (leaf == "filter_type")
            return value < 0.5f ? "Ladder" : value < 1.5f ? "Band-pass" : "High-pass";
        if (leaf == "source")
            return sources[juce::jlimit (0, sources.size() - 1, juce::roundToInt (value))];
        if (leaf == "destination")
            return destinations[juce::jlimit (0, destinations.size() - 1, juce::roundToInt (value))];
        if (leaf.startsWith ("note_"))
        {
            const int semitones = juce::roundToInt (value);
            return (semitones > 0 ? "+" : "") + juce::String (semitones);
        }
        if (maximum >= 1000.0f && value >= 1000.0f)
            return juce::String (value / 1000.0f, 2) + " kHz";

        const int decimals = step >= 1.0f ? 0 : step >= 0.1f ? 1 : 2;
        // Avoid "-0.00" for values that round to zero.
        if (std::abs (value) < 0.5f * std::pow (10.0f, float (-decimals)))
            value = 0.0f;
        auto unit = units.find (leaf);
        return juce::String (value, decimals) + (unit != units.end() ? " " + juce::String::fromUTF8 (unit->second) : juce::String());
    }
};

class Model
{
public:
    explicit Model (juce::AudioProcessor& processor)
    {
        for (auto* p : processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr)
                continue;

            // The name is the Faust label. Only the routing controls repeat one,
            // so their key adds the "Route_i" group read from the ID, whether
            // the ID is a shortname (Route_0_depth) or a full path (…/Route_0/depth).
            const auto leaf = ranged->getName (128);
            // Note parameters belong to the polyphonic engine, not to timbres.
            if (leaf == "freq" || leaf == "gain" || leaf == "gate" || leaf == "Panic")
                continue;

            const auto key = (leaf == "source" || leaf == "destination" || leaf == "depth")
                                 ? ranged->getParameterID().fromLastOccurrenceOf ("Route_", true, false)
                                           .initialSectionContainingOnly ("Route_0123456789")
                                           .trimCharactersAtEnd ("_")
                                       + "/" + leaf
                                 : leaf;
            if (auto* existing = find (key))
            {
                existing->targets.push_back (ranged);
                continue;
            }

            auto param = std::make_unique<Param>();
            param->parameter = ranged;
            param->targets = { ranged };
            param->leaf = leaf;
            param->key = key;
            const auto range = ranged->getNormalisableRange();
            param->minimum = range.start;
            param->maximum = range.end;
            param->defaultValue = ranged->convertFrom0to1 (ranged->getDefaultValue());
            param->isToggle = dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr;

            const auto stepKey = leaf.startsWith ("note_") ? juce::String ("source") : leaf;
            if (auto found = steps.find (stepKey); found != steps.end())
                param->step = found->second;
            const bool discrete = leaf.startsWith ("note_") || leaf == "source" || leaf == "destination"
                                  || leaf == "sequence_octaves" || leaf == "delay_beats";
            param->interval = discrete ? param->step : 0.0f;
            // Same rule as the web sliders: wide, strictly positive ranges travel
            // logarithmically, so attack times and cutoff stay precise at the low end.
            param->logarithmic = param->minimum > 0.0f && param->maximum / param->minimum > 100.0f
                                 && ! leaf.contains ("ratio");
            order.push_back (param.get());
            params[param->key] = std::move (param);
        }
    }

    Param* find (const juce::String& key) const
    {
        auto found = params.find (key);
        return found != params.end() ? found->second.get() : nullptr;
    }

    const std::vector<Param*>& all() const { return order; }

private:
    std::map<juce::String, std::unique_ptr<Param>> params;
    std::vector<Param*> order;
};

//==============================================================================
// The preset bank is presets/patches.json, compiled in as binary data: the
// native, web and offline renders keep sharing a single source.
struct Preset
{
    juce::String id, name, category, description;
    std::map<juce::String, float> values;
};

std::vector<Preset> loadPresets()
{
    const std::map<juce::String, juce::StringArray> describe {
        { "nebula", { "Nebula", "PAD / ANALOG", "A slow, luminous pad with an expansive sense of space." } },
        { "glass", { "Glass", "KEYS / FM", "Shards of glass, blending FM bells with inharmonic overtones." } },
        { "acid", { "Acid", "SEQUENCE / BASS", "A biting filter. Sixteen steps. A restless bass line." } },
        { "dark_brass", { "Dark Brass", "BRASS / HYBRID", "A warm, dense breath shaped by a moving filter." } },
        { "orbital", { "Orbital", "TEXTURE / EXPERIMENTAL", "Metal, ring modulation, and weightless echoes." } },
        { "crystal_sequence", { "Crystal Sequence", "SEQUENCE / DIGITAL", "A crystalline arpeggio unfolding across a stereo space." } },
    };

    std::vector<Preset> presets;
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::patches_json, BinaryData::patches_jsonSize));
    if (auto* object = json.getDynamicObject())
    {
        for (const auto& property : object->getProperties())
        {
            Preset preset;
            preset.id = property.name.toString();
            auto text = describe.find (preset.id);
            preset.name = text != describe.end() ? text->second[0] : preset.id;
            preset.category = text != describe.end() ? text->second[1] : juce::String();
            preset.description = text != describe.end() ? text->second[2] : juce::String();
            if (auto* values = property.value.getDynamicObject())
                for (const auto& value : values->getProperties())
                    preset.values[value.name.toString()] = float (value.value);
            presets.push_back (std::move (preset));
        }
    }
    return presets;
}

//==============================================================================
class Palette : public juce::LookAndFeel_V4
{
public:
    Palette()
    {
        setColour (juce::PopupMenu::backgroundColourId, colour::panel);
        setColour (juce::PopupMenu::textColourId, colour::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::cardActive);
        setColour (juce::PopupMenu::highlightedTextColourId, colour::accent);
        setColour (juce::ComboBox::textColourId, colour::text);
        setColour (juce::TextButton::textColourOffId, colour::text);
        setColour (juce::TextButton::textColourOnId, colour::accent);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 7; }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                           float, float, juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const auto centreY = float (y) + float (height) * 0.5f;
        const juce::Rectangle<float> track (float (x), centreY - 2.0f, float (width), 4.0f);
        g.setColour (colour::track);
        g.fillRoundedRectangle (track, 2.0f);

        // Bipolar ranges (route depths) fill from zero, the others from the left.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto origin = bipolar ? float (slider.getPositionOfValue (0.0)) : float (x);
        g.setColour (colour::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (origin, position), track.getY(),
                                                        std::abs (position - origin), track.getHeight()),
                                2.0f);

        const bool active = slider.isMouseOverOrDragging();
        const auto radius = active ? 7.0f : 6.0f;
        if (active)
        {
            g.setColour (colour::accent.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (radius * 3.4f, radius * 3.4f).withCentre ({ position, centreY }));
        }
        auto thumb = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ position, centreY });
        g.setColour (colour::accent);
        g.fillEllipse (thumb);
        g.setColour (colour::background.withAlpha (0.7f));
        g.drawEllipse (thumb.reduced (0.5f), 1.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? colour::cardActive : highlighted ? juce::Colour (0xff2b3c30) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (highlighted ? juce::Colour (0xff7e9884) : colour::buttonBorder);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override
    {
        // Single-glyph buttons (reset) read better larger.
        if (button.getButtonText().length() == 1)
            return sans (juce::jmin (19.0f, float (buttonHeight) * 0.6f));
        return sans (juce::jmin (12.0f, float (buttonHeight) * 0.45f));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (float (width), float (height)).reduced (0.5f);
        g.setColour (box.isMouseOver (true) ? juce::Colour (0xff2a3b2e) : juce::Colour (0xff233126));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (juce::Colour (0xff3e5143));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        juce::Path chevron;
        const auto cx = float (width) - 12.0f, cy = float (height) * 0.5f;
        chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (colour::muted);
        g.strokePath (chevron, juce::PathStrokeType (1.3f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return sans (11.5f); }
    juce::Font getPopupMenuFont() override { return sans (13.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};

//==============================================================================
// Controls read their parameter back on every timer tick, so host automation,
// preset recall, and state restoration all show up without listeners.
struct Refreshable
{
    virtual ~Refreshable() = default;
    virtual void refresh() = 0;
};

class ParamSlider : public juce::Component, public Refreshable
{
public:
    // compact: no title, value on the right (routing table rows).
    ParamSlider (Param& p, bool isCompact = false) : param (p), compact (isCompact)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        const auto interval = double (param.interval);
        auto snapToStep = [interval] (double start, double end, double value) {
            return interval > 0.0 ? juce::jlimit (start, end, start + interval * std::round ((value - start) / interval)) : value;
        };
        if (param.logarithmic)
            slider.setNormalisableRange ({ param.minimum, param.maximum,
                                           [] (double start, double end, double proportion) { return start * std::pow (end / start, proportion); },
                                           [] (double start, double end, double value) { return std::log (value / start) / std::log (end / start); },
                                           snapToStep });
        else
            slider.setNormalisableRange ({ param.minimum, param.maximum,
                                           [] (double start, double end, double proportion) { return start + (end - start) * proportion; },
                                           [] (double start, double end, double value) { return (value - start) / (end - start); },
                                           snapToStep });
        slider.setDoubleClickReturnValue (true, param.defaultValue);
        slider.setValue (param.get(), juce::dontSendNotification);
        slider.onDragStart = [this] { param.beginGesture(); };
        slider.onDragEnd = [this] { param.endGesture(); };
        slider.onValueChange = [this] {
            param.set (float (slider.getValue()));
            updateText();
        };
        slider.setTitle (param.title());
        addAndMakeVisible (slider);
        updateText();
    }

    void refresh() override
    {
        if (! slider.isMouseButtonDown())
            slider.setValue (param.get(), juce::dontSendNotification);
        updateText();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (compact)
        {
            g.setColour (colour::amber);
            g.setFont (mono (10.5f));
            g.drawText (valueText, bounds.removeFromRight (40.0f), juce::Justification::centredRight);
            return;
        }
        auto top = bounds.removeFromTop (15.0f);
        g.setColour (colour::amber);
        g.setFont (mono (10.5f));
        g.drawText (valueText, top, juce::Justification::centredRight);
        g.setColour (colour::label);
        g.setFont (sans (12.0f));
        g.drawText (param.title(), top.withTrimmedRight (textWidth (mono (10.5f), valueText) + 6.0f),
                    juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        if (compact)
            slider.setBounds (bounds.withTrimmedRight (46));
        else
            // Let the thumb overhang the column edges so the track spans the title width.
            slider.setBounds (bounds.withTrimmedTop (17).withHeight (18).expanded (7, 0));
    }

private:
    void updateText()
    {
        auto text = param.format (float (slider.isMouseButtonDown() ? slider.getValue() : param.get()));
        if (text != valueText)
        {
            valueText = text;
            repaint();
        }
    }

    Param& param;
    bool compact;
    juce::Slider slider;
    juce::String valueText;
};

// On/off pill used for the Faust checkboxes (only "sequencer").
class Switch : public juce::Component, public Refreshable
{
public:
    explicit Switch (Param& p) : param (p)
    {
        setRepaintsOnMouseActivity (true);
        setTitle (param.title());
    }

    void refresh() override
    {
        const bool now = param.get() > 0.5f;
        if (now != on)
        {
            on = now;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()))
        {
            param.set (on ? 0.0f : 1.0f);
            refresh();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto top = bounds.removeFromTop (15.0f);
        g.setColour (colour::amber);
        g.setFont (mono (10.5f));
        g.drawText (on ? "ON" : "OFF", top, juce::Justification::centredRight);
        g.setColour (colour::label);
        g.setFont (sans (12.0f));
        g.drawText (param.title(), top, juce::Justification::centredLeft);

        auto pill = juce::Rectangle<float> (34.0f, 16.0f).withPosition (0.0f, top.getBottom() + 3.0f);
        g.setColour (on ? colour::accent : colour::track);
        g.fillRoundedRectangle (pill, 8.0f);
        if (isMouseOver())
        {
            g.setColour (colour::accent.withAlpha (0.35f));
            g.drawRoundedRectangle (pill.expanded (2.0f), 10.0f, 1.0f);
        }
        auto knob = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ on ? pill.getRight() - 8.0f : pill.getX() + 8.0f, pill.getCentreY() });
        g.setColour (on ? colour::background : colour::muted);
        g.fillEllipse (knob);
    }

private:
    Param& param;
    bool on = false;
};

// ADSR outline. Times are drawn on a square-root scale so a 3 ms attack and an
// 8 s release both stay readable; missing parameters use the DSP constants.
class EnvelopeView : public juce::Component, public Refreshable
{
public:
    EnvelopeView (Param* a, Param* d, Param* s, Param* r, std::array<float, 4> constants)
        : params { a, d, s, r }, values (constants) {}

    void refresh() override
    {
        bool changed = false;
        for (size_t i = 0; i < 4; ++i)
            if (params[i] != nullptr && ! juce::approximatelyEqual (values[i], params[i]->get()))
            {
                values[i] = params[i]->get();
                changed = true;
            }
        if (changed)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colour::scope);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (colour::line);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        auto area = bounds.reduced (10.0f, 9.0f);
        const float attack = std::sqrt (values[0]), decay = std::sqrt (values[1]), release = std::sqrt (values[3]);
        const float hold = 0.25f * (attack + decay + release) + 0.25f;
        const float scale = area.getWidth() / (attack + decay + hold + release);
        const float sustainY = area.getBottom() - values[2] * area.getHeight();

        const juce::Point<float> points[] = {
            { area.getX(), area.getBottom() },
            { area.getX() + attack * scale, area.getY() },
            { area.getX() + (attack + decay) * scale, sustainY },
            { area.getX() + (attack + decay + hold) * scale, sustainY },
            { area.getRight(), area.getBottom() },
        };
        juce::Path outline;
        outline.startNewSubPath (points[0]);
        for (int i = 1; i < 5; ++i)
            outline.lineTo (points[i]);

        auto fill = outline;
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour::accent.withAlpha (0.22f), 0.0f, area.getY(),
                                                 colour::accent.withAlpha (0.0f), 0.0f, area.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour::accent);
        g.strokePath (outline, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
        for (int i = 1; i < 4; ++i)
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (points[i]));
    }

private:
    std::array<Param*, 4> params;
    std::array<float, 4> values;
};

// The sixteen sequencer intervals as bipolar bars: click or drag across the
// steps to draw a pattern, double-click to restore a step's default.
class StepGrid : public juce::Component, public Refreshable
{
public:
    explicit StepGrid (std::vector<Param*> stepParams) : params (std::move (stepParams))
    {
        values.resize (params.size());
        refresh();
    }

    void refresh() override
    {
        bool changed = false;
        for (size_t i = 0; i < params.size(); ++i)
        {
            const int value = juce::roundToInt (params[i]->get());
            changed |= std::exchange (values[i], value) != value;
        }
        if (changed)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bars = barArea();
        for (int i = 0; i < int (params.size()); ++i)
        {
            auto column = columnBounds (i);
            g.setColour (i == hovered ? colour::hover : (i / 4) % 2 == 0 ? juce::Colour (0xff131c18) : juce::Colour (0xff111915));
            g.fillRoundedRectangle (column, 3.0f);

            g.setFont (mono (9.0f));
            g.setColour (i % 4 == 0 ? colour::label : colour::muted.withAlpha (0.7f));
            g.drawText (juce::String (i + 1).paddedLeft ('0', 2), column.withHeight (16.0f), juce::Justification::centred);

            const float zero = bars.getCentreY();
            const float extent = values[size_t (i)] / 24.0f * bars.getHeight() * 0.5f;
            auto bar = juce::Rectangle<float> (column.getX() + 5.0f, juce::jmin (zero, zero - extent),
                                               column.getWidth() - 10.0f, juce::jmax (2.0f, std::abs (extent)));
            g.setColour ((values[size_t (i)] < 0 ? colour::amber : colour::accent).withAlpha (values[size_t (i)] == 0 ? 0.45f : 0.9f));
            g.fillRoundedRectangle (bar, 2.0f);

            g.setColour (colour::amber);
            g.setFont (mono (10.0f));
            g.drawText (params[size_t (i)]->format (float (values[size_t (i)])),
                        column.withTrimmedTop (column.getHeight() - 17.0f), juce::Justification::centred);
        }
        g.setColour (colour::line);
        g.drawHorizontalLine (juce::roundToInt (bars.getCentreY()), bars.getX(), bars.getRight());
    }

    void mouseMove (const juce::MouseEvent& e) override { setHovered (stepAt (e.position.x)); }
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

    void mouseDown (const juce::MouseEvent& e) override { edit (e); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (active >= 0)
            params[size_t (active)]->endGesture();
        active = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto* param = params[size_t (stepAt (e.position.x))];
        param->set (param->defaultValue);
        refresh();
    }

private:
    juce::Rectangle<float> columnBounds (int i) const
    {
        const float width = float (getWidth()) / float (params.size());
        return { float (i) * width + 2.0f, 0.0f, width - 4.0f, float (getHeight()) };
    }

    juce::Rectangle<float> barArea() const { return getLocalBounds().toFloat().withTrimmedTop (20.0f).withTrimmedBottom (21.0f); }

    int stepAt (float x) const
    {
        return juce::jlimit (0, int (params.size()) - 1, int (x / float (getWidth()) * float (params.size())));
    }

    void setHovered (int step)
    {
        if (std::exchange (hovered, step) != step)
            repaint();
    }

    void edit (const juce::MouseEvent& e)
    {
        const int step = stepAt (e.position.x);
        if (step != active)
        {
            if (active >= 0)
                params[size_t (active)]->endGesture();
            active = step;
            params[size_t (active)]->beginGesture();
        }
        const auto bars = barArea();
        params[size_t (step)]->set (float (std::round ((bars.getCentreY() - e.position.y) / (bars.getHeight() * 0.5f) * 24.0f)));
        setHovered (step);
        refresh();
    }

    std::vector<Param*> params;
    std::vector<int> values;
    int hovered = -1, active = -1;
};

// Eight routes: source × signed depth → destination, as in web/app.js.
class RouteTable : public juce::Component, public Refreshable
{
public:
    explicit RouteTable (const Model& model)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto row = std::make_unique<Row>();
            const auto prefix = "Route_" + juce::String (i) + "/";
            row->source = model.find (prefix + "source");
            row->destination = model.find (prefix + "destination");
            auto* depth = model.find (prefix + "depth");
            if (row->source == nullptr || row->destination == nullptr || depth == nullptr)
                continue;

            for (const auto& entry : { std::tuple { &row->sourceBox, row->source, &sources },
                                       std::tuple { &row->destinationBox, row->destination, &destinations } })
            {
                auto* box = std::get<0> (entry);
                auto* param = std::get<1> (entry);
                box->addItemList (*std::get<2> (entry), 1);
                box->setWantsKeyboardFocus (false);
                box->setMouseClickGrabsKeyboardFocus (false);
                box->onChange = [box, param] { param->set (float (box->getSelectedId() - 1)); };
                addAndMakeVisible (*box);
            }
            row->depth = std::make_unique<ParamSlider> (*depth, true);
            addAndMakeVisible (*row->depth);
            rows.push_back (std::move (row));
        }
        refresh();
    }

    void refresh() override
    {
        for (auto& row : rows)
        {
            for (auto [box, param] : { std::pair { &row->sourceBox, row->source },
                                       std::pair { &row->destinationBox, row->destination } })
                if (! box->isPopupActive())
                    box->setSelectedId (juce::roundToInt (param->get()) + 1, juce::dontSendNotification);
            row->depth->refresh();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto header = getLocalBounds().removeFromTop (headerHeight).toFloat();
        g.setFont (mono (9.0f, 0.08f));
        g.setColour (colour::muted);
        const char* titles[] = { "ROUTE", "SOURCE", "DESTINATION", "DEPTH" };
        for (int c = 0; c < 4; ++c)
            g.drawText (titles[c], columnX (c), header.getY(), 120.0f, header.getHeight(), juce::Justification::centredLeft);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto y = float (headerHeight) + float (i) * rowHeight();
            g.setColour (colour::line.withAlpha (0.8f));
            g.drawHorizontalLine (juce::roundToInt (y), 0.0f, float (getWidth()));
            g.setColour (colour::label);
            g.setFont (mono (10.5f));
            g.drawText (juce::String (int (i) + 1).paddedLeft ('0', 2), 2.0f, y, 40.0f, rowHeight(), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const int y = headerHeight + juce::roundToInt (float (i) * rowHeight());
            const int h = juce::roundToInt (rowHeight());
            auto cell = [&] (int c) {
                return juce::Rectangle<int> (juce::roundToInt (columnX (c)), y, juce::roundToInt (columnX (c + 1) - columnX (c)) - 12, h).reduced (0, 3);
            };
            rows[i]->sourceBox.setBounds (cell (1));
            rows[i]->destinationBox.setBounds (cell (2));
            rows[i]->depth->setBounds (cell (3).withWidth (int (float (getWidth()) - columnX (3))));
        }
    }

private:
    struct Row
    {
        Param* source = nullptr;
        Param* destination = nullptr;
        juce::ComboBox sourceBox, destinationBox;
        std::unique_ptr<ParamSlider> depth;
    };

    static constexpr int headerHeight = 18;
    float rowHeight() const { return float (getHeight() - headerHeight) / 8.0f; }

    float columnX (int column) const
    {
        const float fractions[] = { 0.0f, 0.09f, 0.37f, 0.65f, 1.0f };
        return fractions[column] * float (getWidth());
    }

    std::vector<std::unique_ptr<Row>> rows;
};

//==============================================================================
// A tab page: columns with a heading, a stack of controls, and optionally an
// envelope drawing on top or a dedicated editor filling the rest.
struct Column
{
    juce::String title;
    std::vector<juce::String> keys;
    float weight = 1.0f;
    std::array<juce::String, 4> envelope {};
    std::array<float, 4> envelopeConstants {};
    enum class Extra { none, routes, steps } extra = Extra::none;
};

class Page : public juce::Component
{
public:
    Page (const Model& model, std::vector<Column> layout) : columns (std::move (layout))
    {
        for (auto& column : columns)
        {
            Built built;
            if (column.envelope[1].isNotEmpty())
            {
                auto param = [&] (int i) { return column.envelope[size_t (i)].isEmpty() ? nullptr : model.find (column.envelope[size_t (i)]); };
                built.envelope = own (std::make_unique<EnvelopeView> (param (0), param (1), param (2), param (3), column.envelopeConstants));
            }
            for (const auto& key : column.keys)
                if (auto* param = model.find (key))
                    built.controls.push_back (param->isToggle ? own (std::make_unique<Switch> (*param))
                                                              : own (std::make_unique<ParamSlider> (*param)));
            if (column.extra == Column::Extra::routes)
                built.extra = own (std::make_unique<RouteTable> (model));
            if (column.extra == Column::Extra::steps)
            {
                std::vector<Param*> stepParams;
                for (int i = 0; i < 16; ++i)
                    if (auto* param = model.find ("note_" + juce::String (i)))
                        stepParams.push_back (param);
                if (stepParams.size() == 16)
                    built.extra = own (std::make_unique<StepGrid> (std::move (stepParams)));
            }
            built.column = &column;
            layoutItems.push_back (std::move (built));
        }
    }

    void refresh()
    {
        for (auto* item : refreshables)
            item->refresh();
    }

    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < layoutItems.size(); ++i)
        {
            auto area = columnArea (i);
            g.setFont (mono (9.5f, 0.12f));
            g.setColour (juce::Colour (0xff9aac96));
            g.drawText (layoutItems[i].column->title, area.removeFromTop (14), juce::Justification::centredLeft);
            g.setColour (colour::line);
            g.drawHorizontalLine (area.getY() + 5, float (area.getX()), float (area.getRight()));
        }
    }

    void resized() override
    {
        for (size_t i = 0; i < layoutItems.size(); ++i)
        {
            auto area = columnArea (i).withTrimmedTop (28);
            auto& item = layoutItems[i];
            if (item.envelope != nullptr)
            {
                item.envelope->setBounds (area.removeFromTop (60));
                area.removeFromTop (10);
            }
            for (auto* control : item.controls)
            {
                control->setBounds (area.removeFromTop (36));
                area.removeFromTop (4);
            }
            if (item.extra != nullptr)
                item.extra->setBounds (area);
        }
    }

private:
    struct Built
    {
        const Column* column = nullptr;
        juce::Component* envelope = nullptr;
        std::vector<juce::Component*> controls;
        juce::Component* extra = nullptr;
    };

    template <typename Type>
    juce::Component* own (std::unique_ptr<Type> component)
    {
        auto* raw = component.get();
        refreshables.push_back (raw);
        addAndMakeVisible (*raw);
        owned.push_back (std::move (component));
        return raw;
    }

    juce::Rectangle<int> columnArea (size_t index) const
    {
        constexpr float gap = 26.0f;
        float total = 0.0f, before = 0.0f;
        for (size_t i = 0; i < columns.size(); ++i)
        {
            total += columns[i].weight;
            if (i < index)
                before += columns[i].weight;
        }
        const float unit = (float (getWidth()) - gap * float (columns.size() - 1)) / total;
        return juce::Rectangle<float> (before * unit + gap * float (index), 0.0f,
                                       columns[index].weight * unit, float (getHeight()))
            .toNearestInt();
    }

    std::vector<Column> columns;
    std::vector<Built> layoutItems;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Refreshable*> refreshables;
};

struct TabSpec
{
    juce::String name;
    const char* hint; // UTF-8
    std::vector<Column> columns;
};

std::vector<TabSpec> tabSpecs()
{
    using Extra = Column::Extra;
    return {
        { "Engines", "Analog × FM × additive",
          { { "MIX", { "va_level", "fm_level", "add_level", "sub_level", "noise_level" } },
            { "ANALOG", { "morph", "detune_cents", "pulse_width" } },
            { "FM", { "fm_index", "fm_ratio", "fm_parallel" } },
            { "ADDITIVE", { "brightness", "inharmonicity" } },
            { "COLOR", { "fold", "drive", "ring_mod" } } } },
        { "Filter & ADSR", "Ladder · band-pass · high-pass",
          { { "FILTER", { "cutoff", "resonance", "filter_type" } },
            { "TRACKING", { "filter_env_octaves", "keytrack", "velocity_cutoff_octaves" } },
            { "AMP ENVELOPE", { "amp_attack", "amp_decay", "amp_sustain", "amp_release" }, 1.0f,
              { "amp_attack", "amp_decay", "amp_sustain", "amp_release" } },
            { "FILTER ENVELOPE", { "filter_attack", "filter_decay", "filter_sustain", "filter_release" }, 1.0f,
              { "filter_attack", "filter_decay", "filter_sustain", "filter_release" } },
            // The percussive envelope is fixed in aether.dsp except for its decay.
            { "MOD ENVELOPE", { "mod_decay" }, 1.0f, { "", "mod_decay", "", "" }, { 0.003f, 1.2f, 0.0f, 0.2f } } } },
        { "Modulation", "8 routes · 9 sources · 6 destinations",
          { { "SOURCES", { "lfo1_hz", "lfo2_hz", "macro1", "macro2", "stereo_spread" }, 0.9f },
            { "ROUTING MATRIX", {}, 2.6f, {}, {}, Extra::routes } } },
        { "Effects", "Ensemble → phaser → ping-pong → space → master",
          { { "ENSEMBLE", { "chorus_mix", "chorus_rate", "chorus_depth_ms" } },
            { "PHASER", { "phaser_mix", "phaser_rate" } },
            { "PING PONG", { "delay_mix", "delay_feedback", "delay_beats", "delay_tone" } },
            { "SPACE", { "reverb_mix", "reverb_seconds", "predelay_ms" } },
            { "MASTER", { "bass_db", "air_db", "master_db", "width" } } } },
        { "Sequencer", "16 steps · drag to draw intervals",
          { { "TRANSPORT", { "sequencer", "bpm", "step_gate", "sequence_octaves" }, 0.9f },
            { "STEPS", {}, 2.6f, {}, {}, Extra::steps } } },
    };
}

//==============================================================================
class Scope : public juce::Component
{
public:
    explicit Scope (AetherBridge& b) : bridge (b) {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (colour::scopeGrid);
        for (float x = 0.0f; x < bounds.getWidth(); x += bounds.getWidth() / 20.0f)
            g.drawVerticalLine (juce::roundToInt (x), 0.0f, bounds.getHeight());
        for (float y = bounds.getHeight() / 6.0f; y < bounds.getHeight(); y += bounds.getHeight() / 6.0f)
            g.drawHorizontalLine (juce::roundToInt (y), 0.0f, bounds.getWidth());

        // Show the 1024 samples that follow a rising zero crossing so periodic
        // sounds stand still; fall back to the latest samples otherwise.
        constexpr int shown = 1024, size = AetherBridge::scopeSize;
        const int end = bridge.writePosition.load (std::memory_order_acquire);
        auto sample = [&] (int i) { return bridge.scope[size_t (((i % size) + size) % size)].load (std::memory_order_relaxed); };
        int start = end - shown;
        for (int i = end - 2 * shown; i < end - shown; ++i)
            if (sample (i - 1) <= 0.0f && sample (i) > 0.0f)
            {
                start = i;
                break;
            }

        // The ×4 display gain only affects drawing, as in the web scope.
        juce::Path trace;
        const float centre = bounds.getHeight() * 0.54f, amplitude = bounds.getHeight() * 0.36f;
        for (int i = 0; i < shown; ++i)
        {
            const float x = float (i) / float (shown - 1) * bounds.getWidth();
            const float y = centre - juce::jlimit (-1.0f, 1.0f, sample (start + i) * 4.0f) * amplitude;
            if (i == 0)
                trace.startNewSubPath (x, y);
            else
                trace.lineTo (x, y);
        }
        g.setColour (colour::trace.withAlpha (0.18f));
        g.strokePath (trace, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
        g.setColour (colour::trace);
        g.strokePath (trace, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
    }

private:
    AetherBridge& bridge;
};

class PresetCard : public juce::Button
{
public:
    PresetCard (int i, const Preset& p) : juce::Button (p.name), index (i), preset (p)
    {
        setClickingTogglesState (false);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const bool active = getToggleState();
        if (active || highlighted)
        {
            g.setColour (active ? colour::cardActive : down ? colour::cardActive.withAlpha (0.6f) : colour::hover);
            g.fillRoundedRectangle (bounds, 6.0f);
        }
        if (active)
        {
            g.setColour (colour::cardBorder);
            g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        }

        g.setFont (mono (10.0f));
        g.setColour (active ? colour::accent : juce::Colour (0xff7e9584));
        g.drawText (juce::String (index + 1).paddedLeft ('0', 2), 12, 0, 26, getHeight(), juce::Justification::centredLeft);

        g.setColour (colour::text);
        g.setFont (sans (13.5f, true));
        g.drawText (preset.name, 44, 13, getWidth() - 74, 17, juce::Justification::centredLeft);
        g.setColour (colour::muted);
        g.setFont (mono (9.0f, 0.04f));
        g.drawText (preset.category, 44, 33, getWidth() - 74, 13, juce::Justification::centredLeft);

        g.setColour (active ? colour::accent : juce::Colour (0xff526157));
        g.setFont (sans (14.0f));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x97"), getWidth() - 30, 0, 18, getHeight(), juce::Justification::centred);
    }

private:
    int index;
    const Preset& preset;
};

class TabButton : public juce::Button
{
public:
    explicit TabButton (const juce::String& name) : juce::Button (name)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const bool active = getToggleState();
        g.setColour (active ? colour::accent : highlighted ? colour::text : colour::muted);
        g.setFont (sans (12.5f));
        g.drawText (getName(), getLocalBounds(), juce::Justification::centred);
        if (active)
        {
            g.setColour (colour::accent);
            g.fillRect (getLocalBounds().removeFromBottom (2));
        }
    }
};

// Cream and charcoal keys from the web keyboard, with octave names on the Cs.
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    explicit Keyboard (juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent (state, horizontalKeyboard)
    {
        setColour (whiteNoteColourId, juce::Colour (0xff080b09));
        setColour (keySeparatorLineColourId, juce::Colours::transparentBlack);
        setColour (shadowColourId, juce::Colours::transparentBlack);
        setScrollButtonsVisible (false);
        setBlackNoteLengthProportion (0.64f);
        setBlackNoteWidthProportion (0.62f);
        setOctaveForMiddleC (4);
        setVelocity (0.8f, false);
        setWantsKeyboardFocus (true);
    }

    void setFirstNote (int note)
    {
        firstNote = note;
        setAvailableRange (note, note + 24);
        // Computer keys A W S E D … start on the first displayed C.
        setKeyPressBaseOctave (note / 12);
        resized();
    }

    void resized() override
    {
        setKeyWidth (float (getWidth()) / 15.0f);
        juce::MidiKeyboardComponent::resized();
        setLowestVisibleKey (firstNote);
    }

    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over,
                        juce::Colour, juce::Colour) override
    {
        auto key = area.reduced (1.0f, 0.0f).withTrimmedBottom (2.0f);
        g.setColour (down ? juce::Colour (0xff6b936d) : juce::Colour (0xff9b9e90));
        g.fillRoundedRectangle (key, 4.0f);
        g.setColour (down ? juce::Colour (0xffb6d3b6) : over ? juce::Colour (0xffe8e8dc) : juce::Colour (0xffdeded0));
        g.fillRoundedRectangle (key.withTrimmedBottom (5.0f), 4.0f);
        g.fillRect (key.withHeight (6.0f));
        if (note % 12 == 0)
        {
            g.setColour (juce::Colour (0xff687265));
            g.setFont (mono (9.0f));
            g.drawText ("C" + juce::String (note / 12 - 1), key.withTrimmedBottom (12.0f).removeFromBottom (12.0f),
                        juce::Justification::centred);
        }
    }

    void drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour) override
    {
        auto key = area.reduced (0.5f, 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (key.translated (0.0f, 3.0f), 3.0f);
        g.setColour (juce::Colour (0xff080c09));
        g.fillRoundedRectangle (key, 3.0f);
        g.setColour (down ? juce::Colour (0xff506c51) : over ? juce::Colour (0xff27322d) : juce::Colour (0xff1b2420));
        g.fillRoundedRectangle (key.reduced (1.0f, 0.0f).withTrimmedBottom (5.0f), 2.5f);
    }

private:
    int firstNote = 48;
};

//==============================================================================
class ContentView : public juce::Component, private juce::Timer
{
public:
    ContentView (juce::AudioProcessor& p, AetherBridge& b)
        : processor (p), bridge (b), model (p), presets (loadPresets()), tabs (tabSpecs()),
          scope (b), keyboard (b.keyboard)
    {
        for (int i = 0; i < int (presets.size()); ++i)
        {
            auto* card = cards.add (new PresetCard (i, presets[size_t (i)]));
            card->onClick = [this, i] { applyPreset (i); };
            addAndMakeVisible (card);
        }
        for (int i = 0; i < int (tabs.size()); ++i)
        {
            auto* button = tabButtons.add (new TabButton (tabs[size_t (i)].name));
            button->onClick = [this, i] { showTab (i); };
            addAndMakeVisible (button);
        }

        for (auto* button : { &playButton, &resetButton, &octaveDown, &octaveUp, &stopButton })
        {
            button->setWantsKeyboardFocus (false);
            button->setMouseClickGrabsKeyboardFocus (false);
            addAndMakeVisible (button);
        }
        playButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6  Play a chord"));
        resetButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x86\xba"));
        resetButton.setTooltip ("Reset preset");
        octaveDown.setButtonText (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
        octaveUp.setButtonText ("+");
        stopButton.setButtonText ("Stop all");
        playButton.onClick = [this] { playChord(); };
        resetButton.onClick = [this] { applyPreset (bridge.selectedPreset); };
        octaveDown.onClick = [this] { setOctave (octave - 1); };
        octaveUp.onClick = [this] { setOctave (octave + 1); };
        stopButton.onClick = [this] { stopAll(); };

        addAndMakeVisible (scope);
        addAndMakeVisible (keyboard);
        keyboard.setFirstNote ((octave + 1) * 12);

        setSize (designWidth, designHeight);
        showTab (bridge.selectedTab);
        updateCards();
        startTimerHz (30);
    }

    ~ContentView() override
    {
        // Pending chord notes would never be released once this view is gone.
        if (demoPending)
            bridge.keyboard.allNotesOff (1);
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (colour::background);
        paintTopBar (g);
        paintLibrary (g);
        paintPatchHeader (g);
        paintDisplay (g);
        paintPanelHeader (g);
        paintPerformance (g);
        paintFooter (g);
    }

    void resized() override
    {
        for (int i = 0; i < cards.size(); ++i)
            cards[i]->setBounds (20, 250 + i * 66, 212, 60);

        playButton.setBounds (main.getRight() - 196, 108, 148, 36);
        resetButton.setBounds (main.getRight() - 40, 108, 40, 36);
        scope.setBounds (displayArea.withHeight (scopeHeight).reduced (1));

        int x = main.getX();
        for (auto* button : tabButtons)
        {
            const int width = juce::roundToInt (textWidth (sans (12.5f), button->getName())) + 4;
            button->setBounds (x, tabsY, width, 38);
            x += width + 28;
        }
        if (page != nullptr)
            page->setBounds (pageArea);

        stopButton.setBounds (main.getRight() - 70, toolbarY, 70, 26);
        octaveUp.setBounds (stopButton.getX() - 40, toolbarY, 26, 26);
        octaveDown.setBounds (octaveUp.getX() - 52, toolbarY, 26, 26);
        keyboard.setBounds (main.getX(), keyboardY, main.getWidth(), 92);
    }

private:
    // Fixed design layout (see designWidth/designHeight).
    static constexpr int topBarHeight = 72, sidebarRight = 252, footerY = 866;
    static constexpr int scopeHeight = 108, tabsY = 354, toolbarY = 710, keyboardY = 746;
    const juce::Rectangle<int> main { 282, 0, designWidth - 282 - 28, designHeight };
    const juce::Rectangle<int> displayArea { 282, 178, designWidth - 282 - 28, 164 };
    const juce::Rectangle<int> pageArea { 282, 428, designWidth - 282 - 28, 262 };

    void paintTopBar (juce::Graphics& g)
    {
        juce::Path logo;
        logo.addRoundedRectangle (28.0f, 14.0f, 42.0f, 44.0f, 11.0f, 11.0f, true, false, false, true);
        g.setColour (juce::Colour (0xff62816c));
        g.strokePath (logo, juce::PathStrokeType (1.0f));
        g.setColour (colour::accent);
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 31.0f, juce::Font::plain).withKerningFactor (-0.1f)));
        g.drawText (juce::CharPointer_UTF8 ("\xc3\x86"), 28, 14, 40, 44, juce::Justification::centred);

        g.setColour (colour::text);
        g.setFont (sans (22.0f, true, 0.16f));
        g.drawText ("AETHER", 86, 15, 200, 26, juce::Justification::centredLeft);
        g.setColour (colour::muted);
        g.setFont (mono (9.0f, 0.18f));
        g.drawText ("HYBRID INSTRUMENT / 01", 86, 43, 220, 12, juce::Justification::centredLeft);

        // Host format badge, then engine status with its LED.
        const juce::String format = processor.wrapperType == juce::AudioProcessor::wrapperType_Undefined
                                        ? juce::String ("FAUST2JUCE")
                                        : juce::String (juce::AudioProcessor::getWrapperTypeDescription (processor.wrapperType)).toUpperCase();
        auto badge = juce::Rectangle<float> (textWidth (mono (10.0f, 0.12f), format) + 28.0f, 30.0f)
                         .withPosition (float (designWidth - 28) - (textWidth (mono (10.0f, 0.12f), format) + 28.0f), 21.0f);
        g.setColour (colour::buttonBorder);
        g.drawRoundedRectangle (badge, 5.0f, 1.0f);
        g.setColour (colour::label);
        g.setFont (mono (10.0f, 0.12f));
        g.drawText (format, badge, juce::Justification::centred);

        const juce::String status = "Engine running  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  "
                                    + juce::String (bridge.numVoices.load()) + " voices";
        const float statusWidth = textWidth (mono (11.0f), status);
        const float statusX = badge.getX() - 26.0f - statusWidth;
        g.setColour (colour::muted);
        g.setFont (mono (11.0f));
        g.drawText (status, juce::Rectangle<float> (statusX, 21.0f, statusWidth + 4.0f, 30.0f), juce::Justification::centredLeft);
        const auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ statusX - 12.0f, 36.0f });
        g.setColour (colour::accent.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
        g.setColour (colour::accent);
        g.fillEllipse (led);

        g.setColour (colour::line);
        g.drawHorizontalLine (topBarHeight, 0.0f, float (designWidth));
    }

    void paintLibrary (juce::Graphics& g)
    {
        g.setColour (colour::line);
        g.drawVerticalLine (sidebarRight, float (topBarHeight), float (footerY));

        g.setFont (mono (10.0f, 0.12f));
        g.setColour (colour::muted);
        g.drawText ("01 / SOUND LIBRARY", 28, 94, 180, 14, juce::Justification::centredLeft);
        g.drawText (juce::String (int (presets.size())).paddedLeft ('0', 2), 180, 94, 48, 14, juce::Justification::centredRight);

        g.setColour (colour::text);
        g.setFont (sans (26.0f, false, -0.035f));
        g.drawText ("One instrument.", 28, 124, 220, 32, juce::Justification::centredLeft);
        g.drawText ("Many worlds.", 28, 156, 220, 32, juce::Justification::centredLeft);
        g.setColour (colour::muted);
        g.setFont (sans (12.5f));
        g.drawText ("Three synthesis engines.", 28, 198, 220, 18, juce::Justification::centredLeft);
        g.drawText ("A space without boundaries.", 28, 217, 220, 18, juce::Justification::centredLeft);

        g.setColour (colour::accent);
        g.fillEllipse (29.0f, 814.0f, 5.0f, 5.0f);
        g.setColour (juce::Colour (0xffadbaaf));
        g.setFont (mono (11.0f));
        g.drawText (juce::CharPointer_UTF8 ("Faust \xc2\xb7 JUCE"), 44, 808, 180, 16, juce::Justification::centredLeft);
        g.setColour (colour::muted);
        g.setFont (sans (11.0f));
        g.drawText ("Sound is synthesized live.", 44, 826, 190, 16, juce::Justification::centredLeft);
    }

    void paintPatchHeader (juce::Graphics& g)
    {
        const int selected = bridge.selectedPreset;
        const bool hasPreset = juce::isPositiveAndBelow (selected, int (presets.size()));
        g.setColour (colour::muted);
        g.setFont (mono (9.5f, 0.16f));
        g.drawText ("CURRENT PATCH", main.getX(), 92, 140, 14, juce::Justification::centredLeft);
        if (modified)
        {
            const auto badge = juce::Rectangle<float> (float (main.getX()) + 128.0f, 91.0f, 62.0f, 16.0f);
            g.setColour (juce::Colour (0xff6e7860));
            g.drawRect (badge, 1.0f);
            g.setColour (colour::amber);
            g.setFont (mono (8.5f, 0.05f));
            g.drawText ("MODIFIED", badge, juce::Justification::centred);
        }

        g.setColour (colour::text);
        g.setFont (sans (40.0f, false, -0.04f));
        g.drawText (hasPreset ? presets[size_t (selected)].name : juce::String ("Init"), main.getX() - 2, 106, 520, 46,
                    juce::Justification::centredLeft);
        g.setColour (colour::muted);
        g.setFont (sans (12.5f));
        g.drawText (hasPreset ? presets[size_t (selected)].description : juce::String ("The default values declared in aether.dsp."),
                    main.getX(), 152, 560, 18, juce::Justification::centredLeft);
    }

    void paintDisplay (juce::Graphics& g)
    {
        const auto panel = displayArea.toFloat();
        g.setColour (colour::scope);
        g.fillRoundedRectangle (panel, 8.0f);

        // Telemetry strip under the scope.
        auto strip = panel.withTrimmedTop (float (scopeHeight));
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (strip.getX(), strip.getY(), strip.getWidth(), strip.getHeight(), 8.0f, 8.0f, false, false, true, true);
            g.reduceClipRegion (clip);
            g.setColour (colour::telemetry);
            g.fillRect (strip);
        }
        g.setColour (colour::telemetryLine);
        g.drawHorizontalLine (juce::roundToInt (strip.getY()), strip.getX(), strip.getRight());
        g.setColour (colour::line);
        g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);

        auto cells = strip.reduced (22.0f, 0.0f);
        const float cellWidth = cells.getWidth() / 4.25f;
        const juce::String labels[] = { "ACTIVE VOICES", "SAMPLE RATE", "AUDIO LOAD", "OUTPUT LEVEL" };
        const std::pair<juce::String, juce::String> values[] = {
            { juce::String (shownVoices), "/ " + juce::String (bridge.numVoices.load()) },
            { shownRate > 0.0 ? juce::String (shownRate / 1000.0, 1) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")), "kHz" },
            { juce::String (juce::roundToInt (shownLoad)), "%" },
        };
        for (int i = 0; i < 4; ++i)
        {
            auto cell = juce::Rectangle<float> (cells.getX() + float (i) * cellWidth, strip.getY() + 14.0f,
                                                i == 3 ? cellWidth * 1.25f : cellWidth - 20.0f, 34.0f);
            g.setColour (juce::Colour (0xff8da28e));
            g.setFont (mono (8.5f, 0.1f));
            g.drawText (labels[i], cell.removeFromTop (12.0f), juce::Justification::centredLeft);
            if (i < 3)
            {
                const auto& [value, unit] = values[i];
                g.setColour (juce::Colour (0xffccdfc5));
                g.setFont (mono (17.0f));
                g.drawText (value, cell, juce::Justification::centredLeft);
                g.setColour (juce::Colour (0xff718470));
                g.setFont (mono (10.5f));
                g.drawText (unit, cell.withTrimmedLeft (textWidth (mono (17.0f), value) + 7.0f), juce::Justification::centredLeft);
            }
            else
            {
                auto meter = cell.withSizeKeepingCentre (cell.getWidth(), 5.0f).translated (0.0f, 2.0f);
                g.setColour (juce::Colour (0xff2e4030));
                g.fillRoundedRectangle (meter, 2.5f);
                g.setColour (shownLevel > 0.85f ? colour::amber : colour::accent);
                g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * shownLevel), 2.5f);
            }
        }
    }

    void paintPanelHeader (juce::Graphics& g)
    {
        g.setColour (colour::line);
        g.drawHorizontalLine (tabsY + 38, float (main.getX()), float (main.getRight()));

        const auto& tab = tabs[size_t (bridge.selectedTab.load())];
        g.setColour (colour::text);
        g.setFont (sans (11.5f, true, 0.08f));
        g.drawText ("02 / " + tab.name.toUpperCase(), main.getX(), 402, 300, 16, juce::Justification::centredLeft);
        g.setColour (colour::muted);
        g.setFont (sans (11.0f));
        g.drawText (juce::String::fromUTF8 (tab.hint), main.getRight() - 400, 402, 400, 16, juce::Justification::centredRight);
    }

    void paintPerformance (juce::Graphics& g)
    {
        g.setColour (colour::line);
        g.drawHorizontalLine (toolbarY - 12, float (main.getX()), float (main.getRight()));
        g.setColour (colour::muted);
        g.setFont (mono (10.0f, 0.12f));
        g.drawText ("03 / PERFORMANCE", main.getX(), toolbarY, 200, 26, juce::Justification::centredLeft);

        g.setFont (sans (11.5f));
        g.drawText ("Octave", octaveDown.getX() - 56, toolbarY, 48, 26, juce::Justification::centredRight);
        g.setColour (colour::text);
        g.setFont (mono (11.5f));
        g.drawText (juce::String (octave), octaveDown.getRight(), toolbarY, octaveUp.getX() - octaveDown.getRight(), 26,
                    juce::Justification::centred);

        g.setColour (juce::Colour (0xff819184));
        g.setFont (sans (10.5f));
        g.drawText ("Play with the mouse, a MIDI controller, or the keys", main.getX(), keyboardY + 98, 400, 14, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffbcc6b8));
        g.setFont (mono (10.5f));
        g.drawText ("A W S E D F T G Y H U J K", main.getX() + 262, keyboardY + 98, 240, 14, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff819184));
        g.setFont (sans (10.5f));
        g.drawText ("MIDI comes from the host", main.getRight() - 240, keyboardY + 98, 240, 14, juce::Justification::centredRight);
    }

    void paintFooter (juce::Graphics& g)
    {
        g.setColour (colour::line);
        g.drawHorizontalLine (footerY, 0.0f, float (designWidth));
        auto row = juce::Rectangle<int> (28, footerY, designWidth - 56, designHeight - footerY);
        g.setFont (mono (9.5f, 0.06f));
        g.setColour (juce::Colour (0xff718475));
        g.drawText (juce::CharPointer_UTF8 ("AETHER  \xc3\x97  FAUST"), row, juce::Justification::centredLeft);
        g.drawText (juce::CharPointer_UTF8 ("Subtractive \xc2\xb7 FM \xc2\xb7 additive synthesis \xc2\xb7 stereo effects"), row,
                    juce::Justification::centred);
        g.drawText ("faust2juce " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " JUCE " + juce::String (JUCE_MAJOR_VERSION)
                        + "." + juce::String (JUCE_MINOR_VERSION),
                    row, juce::Justification::centredRight);
    }

    //==============================================================================
    void showTab (int index)
    {
        index = juce::jlimit (0, int (tabs.size()) - 1, index);
        bridge.selectedTab = index;
        for (int i = 0; i < tabButtons.size(); ++i)
            tabButtons[i]->setToggleState (i == index, juce::dontSendNotification);
        page = std::make_unique<Page> (model, tabs[size_t (index)].columns);
        addAndMakeVisible (*page);
        page->setBounds (pageArea);
        repaint (main.withY (400).withHeight (24));
    }

    // Preset recall is complete, as on the web: controls the preset does not
    // name go back to their defaults, so nothing leaks from the previous sound.
    void applyPreset (int index)
    {
        const Preset* preset = juce::isPositiveAndBelow (index, int (presets.size())) ? &presets[size_t (index)] : nullptr;
        for (auto* param : model.all())
            param->set (target (*param, preset));
        bridge.selectedPreset = preset != nullptr ? index : -1;
        updateCards();
        modified = false;
        repaint (main.withY (80).withHeight (96));
        if (page != nullptr)
            page->refresh();
    }

    static float target (const Param& param, const Preset* preset)
    {
        if (preset != nullptr)
            if (auto found = preset->values.find (param.key); found != preset->values.end())
                return found->second;
        return param.defaultValue;
    }

    void updateCards()
    {
        for (int i = 0; i < cards.size(); ++i)
            cards[i]->setToggleState (i == bridge.selectedPreset, juce::dontSendNotification);
    }

    void setOctave (int newOctave)
    {
        newOctave = juce::jlimit (1, 6, newOctave);
        if (newOctave == octave)
            return;
        bridge.keyboard.allNotesOff (0);
        octave = newOctave;
        keyboard.setFirstNote ((octave + 1) * 12);
        repaint (main.withY (toolbarY).withHeight (26));
    }

    // Same chord and timing as the web demo, through the on-screen keyboard.
    void playChord()
    {
        stopAll();
        const int generation = ++demoGeneration;
        demoPending = true;
        const int notes[] = { 48, 55, 60, 63 };
        for (int i = 0; i < 4; ++i)
        {
            juce::Component::SafePointer<ContentView> self (this);
            const int note = notes[i];
            juce::Timer::callAfterDelay (i * 110, [self, generation, note] {
                if (self != nullptr && self->demoGeneration == generation)
                    self->bridge.keyboard.noteOn (1, note, 0.8f);
            });
            juce::Timer::callAfterDelay (3300 + i * 110, [self, generation, note, last = i == 3] {
                if (self == nullptr || self->demoGeneration != generation)
                    return;
                self->bridge.keyboard.noteOff (1, note, 0.0f);
                if (last)
                    self->demoPending = false;
            });
        }
    }

    void stopAll()
    {
        ++demoGeneration;
        demoPending = false;
        bridge.keyboard.allNotesOff (0);
        bridge.panicRequested = true;
    }

    void timerCallback() override
    {
        if (! focusRequested && keyboard.isShowing())
        {
            keyboard.grabKeyboardFocus();
            focusRequested = true;
        }
        if (page != nullptr)
            page->refresh();

        const int selected = bridge.selectedPreset;
        const Preset* preset = juce::isPositiveAndBelow (selected, int (presets.size())) ? &presets[size_t (selected)] : nullptr;
        bool changed = false;
        for (auto* param : model.all())
            changed |= std::abs (param->get() - target (*param, preset)) > 1.0e-4f * (param->maximum - param->minimum) + 1.0e-6f;
        if (changed != modified)
        {
            modified = changed;
            repaint (main.withY (88).withHeight (22));
        }

        // Peak hold with a short fall, displayed on a square-root scale.
        level = juce::jmax (bridge.peak.exchange (0.0f), level * 0.85f);
        const int voices = bridge.activeVoices;
        const double rate = bridge.sampleRate;
        const float load = float (bridge.load.getLoadAsPercentage());
        const float shown = juce::jmin (1.0f, std::sqrt (level));
        if (voices != shownVoices || rate != shownRate || std::abs (load - shownLoad) > 0.5f || std::abs (shown - shownLevel) > 0.002f)
        {
            shownVoices = voices;
            shownRate = rate;
            shownLoad = load;
            shownLevel = shown;
            repaint (displayArea.withTrimmedTop (scopeHeight));
        }
        scope.repaint();
    }

    juce::AudioProcessor& processor;
    AetherBridge& bridge;
    Model model;
    std::vector<Preset> presets;
    std::vector<TabSpec> tabs;

    juce::OwnedArray<PresetCard> cards;
    juce::OwnedArray<TabButton> tabButtons;
    std::unique_ptr<Page> page;
    juce::TextButton playButton, resetButton, octaveDown, octaveUp, stopButton;
    Scope scope;
    Keyboard keyboard;

    int octave = 3;
    int demoGeneration = 0;
    bool demoPending = false, modified = false, focusRequested = false;
    float level = 0.0f, shownLoad = -1.0f, shownLevel = 0.0f;
    int shownVoices = -1;
    double shownRate = -1.0;
};
} // namespace aether

//==============================================================================
AetherEditor::AetherEditor (juce::AudioProcessor& processor, AetherBridge& bridge)
    : juce::AudioProcessorEditor (processor),
      lookAndFeel (std::make_unique<aether::Palette>())
{
    setLookAndFeel (lookAndFeel.get());
    content = std::make_unique<aether::ContentView> (processor, bridge);
    addAndMakeVisible (*content);

    // The design canvas scales as a whole; start smaller on short screens.
    double scale = 1.0;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jlimit (0.7, 1.0, (display->userArea.getHeight() - 80) / double (aether::designHeight));
    setResizable (true, true);
    setResizeLimits (int (aether::designWidth * 0.6), int (aether::designHeight * 0.6),
                     int (aether::designWidth * 1.6), int (aether::designHeight * 1.6));
    getConstrainer()->setFixedAspectRatio (double (aether::designWidth) / aether::designHeight);
    setSize (juce::roundToInt (aether::designWidth * scale), juce::roundToInt (aether::designHeight * scale));
}

AetherEditor::~AetherEditor()
{
    content = nullptr;
    setLookAndFeel (nullptr);
}

void AetherEditor::paint (juce::Graphics& g)
{
    g.fillAll (aether::colour::background);
}

void AetherEditor::resized()
{
    content->setTransform (juce::AffineTransform::scale (float (getWidth()) / float (aether::designWidth)));
}
