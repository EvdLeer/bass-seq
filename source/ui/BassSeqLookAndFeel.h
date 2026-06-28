#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colour palette taken from the reference (src/ui/global.css).
namespace BassSeqColours
{
    const juce::Colour panelBg     { 0xff1a1c1f };
    const juce::Colour panelBg2    { 0xff25282c };
    const juce::Colour panelBg3    { 0xff2e3236 };
    const juce::Colour panelStroke { 0xff0e1012 };
    const juce::Colour panelEdge   { 0xff3a3f44 };
    const juce::Colour ledAmber    { 0xffffb347 };
    const juce::Colour ledRed      { 0xffff4b4b };
    const juce::Colour ledBlue     { 0xff4fc3f7 };
    const juce::Colour textCol     { 0xffd8dadd };
    const juce::Colour textDim     { 0xff8a8d93 };
    const juce::Colour textFaint   { 0xff5b5e63 };
    const juce::Colour boxedBg     { 0xffc6cdd4 };   // light background of "boxed" groups
    const juce::Colour boxedText   { 0xff20242a };   // dark labels on that light box
    const juce::Colour knobLight   { 0xff2c2c2c };   // pads theme: near-black knob (#222)
    const juce::Colour knobDark    { 0xff050505 };
    const juce::Colour indicator   { 0xfff0f3f6 };
    // Cabinet (pads theme).
    const juce::Colour cabShellHi  { 0xff6b6f70 };
    const juce::Colour cabShellLo  { 0xff303333 };
    const juce::Colour plate       { 0xffccddee };   // light plate behind filter/sat/delay
    const juce::Colour padTop      { 0xffffffff };
    const juce::Colour padBot      { 0xff82c6ff };
    const juce::Colour padInner    { 0xff333333 };
    const juce::Colour digitCol    { 0xfff5f3ea };
}

// Padding (px) around each knob slider so the drop shadow fits within the component
// bounds and is not clipped. Shared with PadsCabinet (which sets the slider bounds).
inline constexpr int KNOB_PAD = 6;

// Native skin for BASS SEQ: draws the dark rotary knobs with a light indicator.
class BassSeqLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BassSeqLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};
