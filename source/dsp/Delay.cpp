#include "Delay.h"

#include <cmath>

namespace
{
    constexpr float DELAY_TIME_MIN_S = 0.04f;
    constexpr float DELAY_TIME_MAX_S = 0.8f;
    constexpr float FEEDBACK_MAX     = 0.85f;

    float expMap (float x, float lo, float hi)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return lo * std::pow (hi / lo, x);
    }
}

void FeedbackDelay::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    const int maxSamples = (int) std::ceil ((DELAY_TIME_MAX_S + 0.1) * sampleRate);
    line.setMaximumDelayInSamples (maxSamples);
    line.prepare ({ spec.sampleRate, spec.maximumBlockSize, 1 });
    line.reset();

    delaySamples.reset (sampleRate, 0.02);
    feedback.reset (sampleRate, 0.02);
    level.reset (sampleRate, 0.02);

    delaySamples.setCurrentAndTargetValue ((float) (DELAY_TIME_MIN_S * sampleRate));
    feedback.setCurrentAndTargetValue (0.0f);
    level.setCurrentAndTargetValue (0.0f);
}

void FeedbackDelay::reset()
{
    line.reset();
}

void FeedbackDelay::setEnabled (bool on)
{
    enabled = on;
    level.setTargetValue (enabled ? levelKnob : 0.0f);
}

void FeedbackDelay::setLevelKnob (float x)
{
    levelKnob = juce::jlimit (0.0f, 1.0f, x);
    level.setTargetValue (enabled ? levelKnob : 0.0f);
}

void FeedbackDelay::setTimeKnob (float x)
{
    delaySamples.setTargetValue (expMap (x, DELAY_TIME_MIN_S, DELAY_TIME_MAX_S) * (float) sampleRate);
}

void FeedbackDelay::setFeedbackKnob (float x)
{
    feedback.setTargetValue (juce::jlimit (0.0f, 1.0f, x) * FEEDBACK_MAX);
}

void FeedbackDelay::processMonoAdd (float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float dry = samples[i];

        line.setDelay (delaySamples.getNextValue());
        const float delayed = line.popSample (0);

        line.pushSample (0, dry + delayed * feedback.getNextValue());

        samples[i] = dry + delayed * level.getNextValue();
    }
}
