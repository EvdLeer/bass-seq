#include "PluginEditor.h"

BassSeqEditor::BassSeqEditor (BassSeqProcessor& p)
    : juce::AudioProcessorEditor (p),
      audioProcessor (p),
      cabinet (p),
      settingsOverlay (p)
{
    addAndMakeVisible (cabinet);

    settingsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2f35));
    settingsButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc7ced4));
    settingsButton.onClick = [this] { settingsOverlay.setVisible (true); };
    addAndMakeVisible (settingsButton);

    settingsOverlay.onClose = [this] { settingsOverlay.setVisible (false); };
    addChildComponent (settingsOverlay);   // hidden until the button is clicked

    setResizable (true, true);
    setResizeLimits (PadsCabinet::VIEW_W * 3 / 4, PadsCabinet::VIEW_H * 3 / 4,
                     PadsCabinet::VIEW_W * 2,     PadsCabinet::VIEW_H * 2);
    setSize (PadsCabinet::VIEW_W * 5 / 4, PadsCabinet::VIEW_H * 5 / 4);   // 1025 × 449
}

void BassSeqEditor::paint (juce::Graphics& g)
{
    // Background like the web app page (#14161a → #1a1c1f).
    juce::ColourGradient bg (juce::Colour (0xff14161a), 0.0f, 0.0f,
                             juce::Colour (0xff1a1c1f), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (getLocalBounds());

    // Diffuse drop shadow under the backplate (CSS: 0 2px 8px rgba(0,0,0,0.25))
    // — drawn before the cabinet so only the soft halo around it stays visible.
    if (! deviceBounds.isEmpty())
    {
        juce::Path p;
        p.addRoundedRectangle (deviceBounds, deviceRadius);
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 13, juce::Point<int> (0, 5))
            .drawForPath (g, p);
    }
}

void BassSeqEditor::resized()
{
    auto b = getLocalBounds().toFloat();
    const float fill = juce::jmin (b.getWidth()  / (float) PadsCabinet::VIEW_W,
                                   b.getHeight() / (float) PadsCabinet::VIEW_H);
    const float scale = fill * 0.955f;   // a bit smaller → the device 'floats' with margin + shadow

    cabinet.setBounds (0, 0, PadsCabinet::VIEW_W, PadsCabinet::VIEW_H);
    settingsButton.setBounds (getWidth() - 84, 5, 76, 18);
    settingsOverlay.setBounds (getLocalBounds());

    const float sw = (float) PadsCabinet::VIEW_W * scale;
    const float sh = (float) PadsCabinet::VIEW_H * scale;
    const float offX = (b.getWidth()  - sw) * 0.5f;
    const float offY = (b.getHeight() - sh) * 0.5f;
    const auto  xf   = juce::AffineTransform::scale (scale).translated (offX, offY);
    cabinet.setTransform (xf);

    // Body shell = (3, 13, 814, 342) in cabinet coordinates → screen rect for the shadow.
    deviceBounds = juce::Rectangle<float> (3.0f, 13.0f, 814.0f, 342.0f).transformedBy (xf);
    deviceRadius = 24.0f * scale;
}
