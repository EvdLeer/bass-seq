#pragma once

#include <juce_dsp/juce_dsp.h>

// Bypassable waveshaper (tanh) with drive + tone.
//   tanh(x · preGain) → DC-block (HPF 20 Hz) → tone (LPF) → postGain
// Bypass via a smoothed dry/wet mix (click-free in/out).
class Saturation
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setEnabled (bool on);
    void setDriveKnob (float x);   // 0..1
    void setToneKnob (float x);    // 0..1

    void processMono (float* samples, int numSamples);

private:
    void updateDrive();
    void updateTone();

    double sampleRate = 44100.0;
    float  driveKnob  = 0.4f;
    float  toneKnob   = 0.5f;
    float  preGain    = 1.0f;
    float  postGain   = 1.0f;

    juce::dsp::StateVariableTPTFilter<float> dcBlock;   // highpass 20 Hz
    juce::dsp::StateVariableTPTFilter<float> toneLpf;   // lowpass, adjustable
    juce::SmoothedValue<float> wetMix { 0.0f };         // 0 = dry, 1 = wet
};
