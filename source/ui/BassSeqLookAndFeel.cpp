#include "BassSeqLookAndFeel.h"

using namespace BassSeqColours;

BassSeqLookAndFeel::BassSeqLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, textCol);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textCol);
}

void BassSeqLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider&)
{
    // The slider bounds are padded out by KNOB_PAD; reduce back to the knob box
    // (same visible size as before) so there is room for the shadow.
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                      .reduced ((float) KNOB_PAD + 1.0f);
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto  centre    = bounds.getCentre();
    const float radius    = diameter * 0.5f;
    const auto  knob      = juce::Rectangle<float> (diameter, diameter).withCentre (centre);

    // Soft, diffuse drop shadow (CSS: 0 2px 3px rgba(0,0,0,0.65)) — now fits within
    // the padding and is no longer clipped at the bottom.
    {
        juce::Path kp; kp.addEllipse (knob);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 4, juce::Point<int> (0, 2)).drawForPath (g, kp);
    }

    // Matte, flat body (#222) — no convex gradient, no specular.
    g.setColour (juce::Colour (0xff222222));
    g.fillEllipse (knob);

    // Subtle side lighting along the edges (light left, dark right);
    // the centre stays flat/matte so the knob does not look convex.
    {
        juce::ColourGradient edge (juce::Colours::white.withAlpha (0.15f), knob.getX(), centre.y,
                                   juce::Colours::black.withAlpha (0.22f), knob.getRight(), centre.y, false);
        edge.addColour (0.20, juce::Colours::transparentBlack);
        edge.addColour (0.82, juce::Colours::transparentBlack);
        g.setGradientFill (edge);
        g.fillEllipse (knob);
    }

    // Black border (CSS: 1px rgba(0,0,0,0.82)).
    g.setColour (juce::Colours::black.withAlpha (0.82f));
    g.drawEllipse (knob, 1.0f);

    // Pointer: thin, matte-light line from (almost) the centre to the edge — no glow.
    // A subtle dark shadow beside it gives the line depth (an engraved feel).
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const float lineW  = radius > 18.0f ? 2.2f : 1.6f;
    const auto  base    = centre.getPointOnCircumference (radius * 0.10f, angle);
    const auto  tip     = centre.getPointOnCircumference (radius * 0.92f, angle);
    const auto  perp    = juce::Point<float> (std::cos (angle + juce::MathConstants<float>::halfPi),
                                              std::sin (angle + juce::MathConstants<float>::halfPi)) * 0.7f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));              // shadow side
    g.drawLine ({ base + perp, tip + perp }, lineW);
    g.setColour (juce::Colour (0xffcfd0d2));                          // matte light pointer
    g.drawLine ({ base, tip }, lineW);
}
