#include "Saturation.h"

#include <cmath>

namespace
{
    constexpr float DRIVE_PRE_MIN = 1.0f;
    constexpr float DRIVE_PRE_MAX = 20.0f;
    constexpr float TONE_MIN_HZ   = 800.0f;
    constexpr float TONE_MAX_HZ   = 9000.0f;

    float expMap (float x, float lo, float hi)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return lo * std::pow (hi / lo, x);
    }
}

void Saturation::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    juce::dsp::ProcessSpec mono { spec.sampleRate, spec.maximumBlockSize, 1 };

    dcBlock.prepare (mono);
    dcBlock.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlock.setCutoffFrequency (20.0f);

    toneLpf.prepare (mono);
    toneLpf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    updateDrive();
    updateTone();

    wetMix.reset (sampleRate, 0.01);
    wetMix.setCurrentAndTargetValue (0.0f);
}

void Saturation::reset()
{
    dcBlock.reset();
    toneLpf.reset();
}

void Saturation::setEnabled (bool on)      { wetMix.setTargetValue (on ? 1.0f : 0.0f); }
void Saturation::setDriveKnob (float x)    { driveKnob = x; updateDrive(); }
void Saturation::setToneKnob (float x)     { toneKnob  = x; updateTone(); }

void Saturation::updateDrive()
{
    const float d = juce::jlimit (0.0f, 1.0f, driveKnob);
    preGain  = DRIVE_PRE_MIN + (DRIVE_PRE_MAX - DRIVE_PRE_MIN) * d;
    postGain = 1.0f / (1.0f + d * 0.5f);
}

void Saturation::updateTone()
{
    toneLpf.setCutoffFrequency (expMap (toneKnob, TONE_MIN_HZ, TONE_MAX_HZ));
}

void Saturation::processMono (float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float dry = samples[i];

        float shaped = std::tanh (dry * preGain);
        shaped = dcBlock.processSample (0, shaped);
        shaped = toneLpf.processSample (0, shaped);
        shaped *= postGain;

        const float m = wetMix.getNextValue();
        samples[i] = dry * (1.0f - m) + shaped * m;
    }
}
