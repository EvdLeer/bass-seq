#include "SettingsOverlay.h"

namespace
{
    void styleDark (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2b3036));
        c.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
        c.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff454b52));
        c.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xff9aa3ab));
    }
    void styleDark (juce::Slider& s)
    {
        s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2b3036));
        s.setColour (juce::Slider::trackColourId,      juce::Colour (0xff4f7fb5));
        s.setColour (juce::Slider::thumbColourId,      juce::Colour (0xffd2d8da));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff20242a));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff454b52));
    }
    void styleDark (juce::ToggleButton& t)
    {
        t.setColour (juce::ToggleButton::tickColourId,         juce::Colour (0xff4fc3f7));
        t.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff556069));
    }
}

SettingsOverlay::SettingsOverlay (BassSeqProcessor& p) : proc (p)
{
    auto changed = [this] { if (! loading) pushToProcessor(); };

    inputCh.addItem ("Omni", 1);
    for (int i = 1; i <= 16; ++i) inputCh.addItem (juce::String (i), i + 1);
    for (int i = 1; i <= 16; ++i) outputCh.addItem (juce::String (i), i);
    clockSrc.addItem ("Internal", 1); clockSrc.addItem ("MIDI", 2); clockSrc.addItem ("Auto", 3);
    keyPrio.addItem ("Low", 1); keyPrio.addItem ("High", 2); keyPrio.addItem ("Last", 3);

    for (auto* c : { &inputCh, &outputCh, &clockSrc, &keyPrio })
    {
        styleDark (*c); c->onChange = changed; addAndMakeVisible (*c);
    }
    for (auto* s : { &transpose, &accentThr })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 20);
        styleDark (*s); s->onValueChange = changed; addAndMakeVisible (*s);
    }
    transpose.setRange (-12.0, 12.0, 1.0);
    accentThr.setRange (0.0, 127.0, 1.0);

    for (auto* t : { &localCtrl, &fwdKbd, &fwdSeq, &fwdArp, &fwdClk, &waitEnd })
    {
        t->setButtonText ({}); styleDark (*t); t->onClick = changed; addAndMakeVisible (*t);
    }

    closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff343b42));
    closeBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeBtn);

    setWantsKeyboardFocus (true);
}

void SettingsOverlay::visibilityChanged()
{
    if (isVisible()) { pullFromProcessor(); toFront (true); }
}

void SettingsOverlay::pullFromProcessor()
{
    const juce::ScopedValueSetter<bool> sv (loading, true);
    const auto s = proc.getSettings();
    inputCh.setSelectedId (s.inputChannel == 0 ? 1 : s.inputChannel + 1, juce::dontSendNotification);
    outputCh.setSelectedId (juce::jlimit (1, 16, s.outputChannel), juce::dontSendNotification);
    clockSrc.setSelectedId (s.clockSource + 1, juce::dontSendNotification);
    keyPrio.setSelectedId (s.keyPriority + 1, juce::dontSendNotification);
    transpose.setValue (s.inputTranspose, juce::dontSendNotification);
    accentThr.setValue (s.accentThreshold, juce::dontSendNotification);
    localCtrl.setToggleState (s.localControl, juce::dontSendNotification);
    fwdKbd.setToggleState (s.fwdKeyboard, juce::dontSendNotification);
    fwdSeq.setToggleState (s.fwdSequencer, juce::dontSendNotification);
    fwdArp.setToggleState (s.fwdArp, juce::dontSendNotification);
    fwdClk.setToggleState (s.fwdClock, juce::dontSendNotification);
    waitEnd.setToggleState (s.waitForPatternEnd, juce::dontSendNotification);
}

void SettingsOverlay::pushToProcessor()
{
    GlobalSettings s = proc.getSettings();   // keep fields not visible here
    s.inputChannel    = inputCh.getSelectedId() <= 1 ? 0 : inputCh.getSelectedId() - 1;
    s.outputChannel   = outputCh.getSelectedId();
    s.clockSource     = clockSrc.getSelectedId() - 1;
    s.keyPriority     = keyPrio.getSelectedId() - 1;
    s.inputTranspose  = (int) transpose.getValue();
    s.accentThreshold = (int) accentThr.getValue();
    s.localControl    = localCtrl.getToggleState();
    s.fwdKeyboard     = fwdKbd.getToggleState();
    s.fwdSequencer    = fwdSeq.getToggleState();
    s.fwdArp          = fwdArp.getToggleState();
    s.fwdClock        = fwdClk.getToggleState();
    s.waitForPatternEnd = waitEnd.getToggleState();
    proc.setSettings (s);
}

void SettingsOverlay::resized()
{
    labels.clear();
    const int cw = 380, chh = juce::jmin (getHeight() - 12, 412);
    card = { (getWidth() - cw) / 2, juce::jmax (6, (getHeight() - chh) / 2), cw, chh };

    auto area = card.reduced (16);
    area.removeFromTop (26);   // title
    closeBtn.setBounds (card.getRight() - 78, card.getY() + 8, 60, 20);

    auto section = [&] (const juce::String& t) { labels.push_back ({ t, area.removeFromTop (16), true }); area.removeFromTop (2); };
    auto row = [&] (const juce::String& t, juce::Component& c)
    {
        auto r = area.removeFromTop (22); area.removeFromTop (1);
        c.setBounds (r.removeFromRight (188));
        labels.push_back ({ t, r, false });
    };

    section ("MIDI INPUT");
    row ("Channel",       inputCh);
    row ("Transpose",     transpose);
    row ("Accent vel >",  accentThr);
    row ("Key priority",  keyPrio);
    row ("Local control", localCtrl);
    section ("MIDI OUTPUT");
    row ("Channel",        outputCh);
    row ("Fwd keyboard",   fwdKbd);
    row ("Fwd sequencer",  fwdSeq);
    row ("Fwd arp",        fwdArp);
    row ("Fwd clock",      fwdClk);
    section ("CLOCK / MISC");
    row ("Clock source",     clockSrc);
    row ("Wait pattern end", waitEnd);
}

void SettingsOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition()) && onClose)   // click outside the card -> close
        onClose();
}

void SettingsOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));   // dimmer

    g.setColour (juce::Colour (0xff20242a));
    g.fillRoundedRectangle (card.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff454b52));
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
    g.drawText ("SETTINGS", card.getX() + 18, card.getY() + 8, 200, 22, juce::Justification::centredLeft);

    for (const auto& l : labels)
    {
        if (l.section)
        {
            g.setColour (juce::Colour (0xff4fc3f7));
            g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
            g.drawText (l.text, l.bounds, juce::Justification::bottomLeft);
        }
        else
        {
            g.setColour (juce::Colour (0xffc7ced4));
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawText (l.text, l.bounds, juce::Justification::centredLeft);
        }
    }
}
