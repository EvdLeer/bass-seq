#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <set>
#include <map>
#include "PluginProcessor.h"
#include "BassSeqLookAndFeel.h"
#include "EditController.h"

// Pads theme: pixel-perfect cabinet.
// Fixed 820×359 canvas; the editor scales it to the window via setTransform.
// Knobs are child sliders; pads/sharps/switches/knob-grid are drawn
// and operated via hit-tests in the mouse handlers.
class PadsCabinet : public juce::Component,
                    private juce::Timer
{
public:
    static constexpr int VIEW_W = 820;
    static constexpr int VIEW_H = 359;

    explicit PadsCabinet (BassSeqProcessor&);
    ~PadsCabinet() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;   // help overlay above the knob sliders
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    void timerCallback() override;
    void drawHelp (juce::Graphics&);

    BassSeqProcessor&  proc;
    BassSeqLookAndFeel laf;
    EditController     edit;

    juce::OwnedArray<juce::Slider> knobSliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    int    heldNote      = -1;       // pad/sharp currently pressed with the mouse
    int    sustainedNote = -1;       // note held by HOLD after release (keyboard sustain)

    // computer keyboard
    bool   helpVisible = false;
    std::set<int>      keysDown;     // key codes we have already processed (edge detection)
    std::map<int, int> pcNote;       // piano key code → sounding MIDI note

    // change detection for the timer repaint
    int    lastStep    = -2;
    bool   lastPlaying = false;
    int    lastTempo   = -1;
    int    lastSig     = 0;          // EditController::uiSignature()
    int    lastBlink   = 0;
    bool   blinkOn     = true;       // blink phase for the edit cursor

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadsCabinet)
};
