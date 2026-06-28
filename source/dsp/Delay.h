#pragma once

#include <juce_dsp/juce_dsp.h>

// Bypass-bare feedback-delay (send/return).
//   input → delayLine → wetLevel → output   (+ feedback-loop)
// The dry path runs around it; processMonoAdd only adds the wet signal.
class FeedbackDelay
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setEnabled (bool on);
    void setLevelKnob (float x);     // 0..1, wet-send
    void setTimeKnob (float x);      // 0..1, expMap → 0.04..0.8 s
    void setFeedbackKnob (float x);  // 0..1, ×0.85

    void processMonoAdd (float* samples, int numSamples);   // dry stays, wet added on top

private:
    double sampleRate = 44100.0;
    bool   enabled    = false;
    float  levelKnob  = 0.4f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line;
    juce::SmoothedValue<float> delaySamples { 0.0f };
    juce::SmoothedValue<float> feedback     { 0.0f };
    juce::SmoothedValue<float> level        { 0.0f };
};
