#pragma once

#include "PluginProcessor.h"
#include "ui/PadsCabinet.h"
#include "ui/SettingsOverlay.h"

// Pads theme: the whole UI is the PadsCabinet (fixed 820×359 canvas), which the
// editor places scaled and centered within the window.
class BassSeqEditor : public juce::AudioProcessorEditor
{
public:
    explicit BassSeqEditor (BassSeqProcessor&);
    ~BassSeqEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BassSeqProcessor& audioProcessor;
    PadsCabinet cabinet;
    SettingsOverlay settingsOverlay;
    juce::TextButton settingsButton { "SETTINGS" };

    // Backplate rectangle (body shell) in screen coordinates — for the diffuse
    // drop shadow the editor draws behind the cabinet (3D lift).
    juce::Rectangle<float> deviceBounds;
    float deviceRadius = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSeqEditor)
};
