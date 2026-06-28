#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Settings overlay: exposes the global MIDI/clock settings from
// GlobalSettings (the hardware does this via SysEx/power-on; we provide a
// clean overlay). Dimmed background + dark card, filled from proc.getSettings()
// and written back on every change via proc.setSettings().
class SettingsOverlay : public juce::Component
{
public:
    explicit SettingsOverlay (BassSeqProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // click on the dimmer = close
    void visibilityChanged() override;

    std::function<void()> onClose;

private:
    void pullFromProcessor();    // settings → controls
    void pushToProcessor();      // controls → settings

    BassSeqProcessor& proc;

    juce::ComboBox      inputCh, outputCh, clockSrc, keyPrio;
    juce::Slider        transpose, accentThr;
    juce::ToggleButton  localCtrl, fwdKbd, fwdSeq, fwdArp, fwdClk, waitEnd;
    juce::TextButton    closeBtn { "Close" };

    struct Lbl { juce::String text; juce::Rectangle<int> bounds; bool section; };
    std::vector<Lbl> labels;     // built in resized(), drawn in paint()

    juce::Rectangle<int> card;
    bool loading = false;        // suppress onChange during pullFromProcessor

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsOverlay)
};
