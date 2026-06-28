#include "PadsCabinet.h"

using namespace BassSeqColours;

namespace
{
    struct KnobSpec { const char* param; int x, y, w, h; const char* label; int lx, ly, lw; };
    const KnobSpec KNOBS[] = {
        { "PITCH",     372, 29, 30, 30, "PITCH",     378,  64, 22 },
        { "CUTOFF",    423, 29, 30, 30, "CUTOFF",    424,  64, 28 },
        { "RESONANCE", 475, 29, 30, 30, "RESONANCE", 472,  64, 39 },
        { "ENVMOD",    528, 29, 30, 30, "DEPTH",     530,  64, 25 },
        { "DECAY",     581, 29, 30, 30, "DECAY",     585,  64, 24 },
        { "ACCENT",    634, 29, 30, 30, "AMOUNT",    637,  64, 28 },
        { "DRIVE",     372, 89, 30, 30, "DRIVE",     375, 124, 25 },
        { "TONE",      423, 89, 30, 30, "TONE",      429, 124, 22 },
        { "DLY_LEVEL", 528, 89, 30, 30, "LEVEL",     530, 124, 27 },
        { "DLY_TIME",  581, 89, 30, 30, "TIME",      586, 124, 21 },
        { "DLY_FB",    634, 89, 30, 30, "FEEDBACK",  632, 124, 41 },
        { "MASTER",    706, 56, 43, 43, "MASTER",    711, 102, 35 },
    };

    struct SwitchSpec { const char* param; int x, y, w, h; };
    const SwitchSpec SWITCHES[] = {
        { "WAVE",   323, 34, 20, 20 },
        { "SAT_ON", 323, 94, 20, 20 },
        { "DLY_ON", 481, 94, 20, 20 },
    };

    struct LabelSpec { const char* t; int x, y, w; float size; bool dim; };
    const LabelSpec LABELS[] = {
        { "VCO",        378, 23, 18, 5.0f, false },   // centred on the PITCH knob (387)
        { "FILTER",     475, 21, 31, 5.0f, false },
        { "ENVELOPE",   578, 22, 36, 5.0f, false },
        { "ACCENT",     635, 22, 28, 5.0f, false },   // centred on the AMOUNT knob (649)
        { "SATURATION", 367, 80, 43, 5.0f, false },
        { "DELAY",      558, 80, 28, 5.0f, false },
        { "1/2",        329, 30, 14, 5.0f, true  },
        { "WAVE",       322, 23, 22, 5.0f, false },
        { "ON",         326, 80, 14, 5.0f, false },
        { "OFF",        324,124, 18, 5.0f, true  },
        { "ON",         484, 80, 14, 5.0f, false },
        { "OFF",        482,124, 18, 5.0f, true  },
        { "AUX IN",     223, 72, 21, 5.0f, true  },
        { "SYNC IN",    219,112, 30, 5.0f, true  },
        { "SYNC OUT",   258,112, 34, 5.0f, true  },
    };

    struct Box { int x, y, w, h; };
    const Box JACKS[] = {
        { 220, 41, 28, 28 }, { 260, 41, 28, 28 }, { 220, 81, 28, 28 }, { 260, 81, 28, 28 },
    };
    const Box PLATES[] = { { 421, 28, 139, 32 }, { 316, 88, 139, 32 }, { 474, 88, 192, 32 } };

    struct KeySpec { int midi, x, y, w, h; };
    const KeySpec SHARPS[] = {
        { 37, 89,160,34,84 }, { 39,127,160,34,84 }, { 42,203,161,34,84 }, { 44,241,161,34,84 },
        { 46,279,160,34,84 }, { 49,355,161,34,84 }, { 51,393,161,34,84 }, { 54,469,161,34,84 },
        { 56,507,161,34,84 }, { 58,545,160,34,84 },
    };

    struct PadSpec { int step, midi, x, y, w, h; };
    const PadSpec PADS[] = {
        { 1,35, 33,248,34,84 }, { 2,36, 71,248,34,84 }, { 3,38,108,248,34,84 }, { 4,40,145,248,34,84 },
        { 5,41,182,248,34,84 }, { 6,43,220,248,34,84 }, { 7,45,258,248,34,84 }, { 8,47,296,248,34,84 },
        { 9,48,334,248,34,84 }, {10,50,372,248,34,84 }, {11,52,409,248,34,84 }, {12,53,447,248,34,84 },
        {13,55,485,248,34,84 }, {14,57,523,248,34,84 }, {15,59,561,248,34,84 }, {16,60,599,248,34,84 },
    };

    struct ButtonSpec { const char* id; int x, y, w, h; const char* label; const char* sym; };
    const ButtonSpec BUTTONS[] = {
        { "down",671,210,32,16,"","v" }, { "oct",712,209,34,17,"OCT","" }, { "up",757,210,32,16,"","^" },
        { "tempo_tap",671,237,32,16,"TEMPO TAP","" }, { "play_stop",714,237,31,16,"PLAY/STOP","" }, { "rec_edit",757,237,32,16,"REC/EDIT","" },
        { "pattern",671,264,31,16,"PATTERN","" }, { "save",714,264,31,16,"SAVE","" }, { "clear_rest",757,264,31,16,"CLEAR/REST","" },
        { "gate_length",671,291,31,16,"GATE LENGTH","" }, { "accent",713,291,32,16,"ACCENT","" }, { "ratchet",756,291,32,16,"RATCHET","" },
        { "slide",670,318,32,16,"SLIDE","" }, { "arp",713,318,32,16,"ARP","" }, { "hold",756,318,32,16,"HOLD","" },
    };

    void drawText (juce::Graphics& g, const juce::String& t, int x, int y, int w, float size,
                   juce::Colour c, juce::Justification just = juce::Justification::centred)
    {
        g.setColour (c);
        g.setFont (juce::Font (juce::FontOptions (size)).boldened());   // bold labels (CSS font-weight 800)
        g.drawText (t, juce::Rectangle<int> (x, y, w, (int) std::ceil (size) + 3), just, false);
    }

    // 7-segment character mask (bits: a=1,b=2,c=4,d=8,e=16,f=32,g=64).
    unsigned char segMask (char c)
    {
        static const unsigned char MASK[10] =
            { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
        if (c >= '0' && c <= '9') return MASK[c - '0'];
        if (c == '-')             return 0x40;   // middle bar only (g)
        return 0;
    }

    // 7-segment character at (x,y,w,h).
    void drawSegChar (juce::Graphics& g, float x, float y, float w, float h, char ch,
                      juce::Colour on, juce::Colour off)
    {
        const unsigned char m = segMask (ch);
        const float th  = h * 0.13f;
        const float midY = y + h * 0.5f;
        const float vh  = h * 0.5f - th * 1.2f;

        auto bar = [&] (float bx, float by, float bw, float bh, bool lit)
        {
            g.setColour (lit ? on : off);
            g.fillRoundedRectangle (bx, by, bw, bh, juce::jmin (bw, bh) * 0.42f);
        };
        bar (x + th,        y,                  w - 2 * th, th, m & 1);   // a
        bar (x + th,        midY - th * 0.5f,   w - 2 * th, th, m & 64);  // g
        bar (x + th,        y + h - th,         w - 2 * th, th, m & 8);   // d
        bar (x,             y + th * 0.6f,      th, vh, m & 32);          // f
        bar (x + w - th,    y + th * 0.6f,      th, vh, m & 2);           // b
        bar (x,             midY + th * 0.6f,   th, vh, m & 16);          // e
        bar (x + w - th,    midY + th * 0.6f,   th, vh, m & 4);           // c
    }

    // Right-aligned number in 7-segment style, ending at x=right. Shows a
    // minus sign as a prefix for negative values (octave display).
    void drawSegNumber (juce::Graphics& g, int value, float right, float y, float h,
                        juce::Colour on, juce::Colour off)
    {
        const juce::String s (value);          // may contain a '-'
        const float dw = h * 0.62f, gap = h * 0.16f;
        float cx = right - dw;
        for (int i = s.length() - 1; i >= 0; --i)
        {
            drawSegChar (g, cx, y, dw, h, (char) s[i], on, off);
            cx -= (dw + gap);
        }
    }
}

//==============================================================================
PadsCabinet::PadsCabinet (BassSeqProcessor& p) : proc (p), edit (p)
{
    for (const auto& k : KNOBS)
    {
        auto* s = new juce::Slider();
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setWantsKeyboardFocus (false);
        s->setLookAndFeel (&laf);
        addAndMakeVisible (s);
        knobSliders.add (s);
        knobAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.param, *s));
    }

    setSize (VIEW_W, VIEW_H);   // after creation → resized() places the knobs
    startTimerHz (60);          // smooth step highlight (see the clipped repaint in timerCallback)
    setWantsKeyboardFocus (true);   // computer-keyboard shortcuts
}

PadsCabinet::~PadsCabinet()
{
    stopTimer();
    // Release any hanging / HOLD-sustained notes, otherwise a note stays
    // stuck in keyboardState if the editor closes while a key is active.
    if (heldNote >= 0)      proc.keyboardState.noteOff (1, heldNote, 0.0f);
    if (sustainedNote >= 0) proc.keyboardState.noteOff (1, sustainedNote, 0.0f);
    for (auto* s : knobSliders)
        s->setLookAndFeel (nullptr);
}

void PadsCabinet::resized()
{
    // Stretch each knob slider by KNOB_PAD; the LookAndFeel draws the knob at its
    // original position/size and keeps the padding for the drop shadow.
    for (int i = 0; i < knobSliders.size(); ++i)
        knobSliders[i]->setBounds (KNOBS[i].x - KNOB_PAD, KNOBS[i].y - KNOB_PAD,
                                   KNOBS[i].w + 2 * KNOB_PAD, KNOBS[i].h + 2 * KNOB_PAD);
}

//==============================================================================
void PadsCabinet::paint (juce::Graphics& g)
{
    auto rr = [&g] (Box b, float r, juce::Colour c) { g.setColour (c); g.fillRoundedRectangle ((float) b.x, (float) b.y, (float) b.w, (float) b.h, r); };

    // Body shell (brushed metal) — vertical gradient + bright white inner rim.
    {
        juce::ColourGradient grad (cabShellHi, 0, 13.0f, juce::Colour (0xff5f6667), 0, 355.0f, false);
        grad.addColour (0.38, cabShellLo);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (3.0f, 13.0f, 814.0f, 342.0f, 24.0f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));          // gloss rim (inset 2px)
        g.drawRoundedRectangle (4.0f, 14.0f, 812.0f, 340.0f, 23.0f, 2.0f);
        g.setColour (juce::Colour (0xff141414).withAlpha (0.65f));     // dark outer edge
        g.drawRoundedRectangle (3.0f, 13.0f, 814.0f, 342.0f, 24.0f, 1.0f);
    }
    // Top panel: horizontal gradient (black left → metallic grey right).
    {
        juce::ColourGradient grad (juce::Colour (0xff020202), 6.0f, 0, juce::Colour (0xff323334), 814.0f, 0, false);
        grad.addColour (0.44, juce::Colour (0xff030303));
        grad.addColour (0.63, juce::Colour (0xff101010));
        grad.addColour (0.82, juce::Colour (0xff484a4a));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (6.0f, 17.0f, 808.0f, 121.0f, 16.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));          // top highlight
        g.fillRect (12.0f, 18.0f, 796.0f, 1.0f);
    }
    // Lower panel: brushed metal — diagonal (≈105°) gradient + soft sheen bloom.
    // Only the BOTTOM corners are rounded (CSS: border-radius 0 0 20px 20px); the
    // top is straight and meets the divider line.
    {
        juce::Path lower;
        lower.addRoundedRectangle (4.0f, 135.0f, 812.0f, 218.0f, 18.0f, 18.0f,
                                   false, false, true, true);

        juce::ColourGradient grad (juce::Colour (0xff929798), 4.0f, 150.0f,
                                   juce::Colour (0xff656b6c), 816.0f, 338.0f, false);
        grad.addColour (0.29, juce::Colour (0xff596061));
        grad.addColour (0.55, juce::Colour (0xff4f5455));
        grad.addColour (0.86, juce::Colour (0xff737879));
        g.setGradientFill (grad);
        g.fillPath (lower);

        const float bx = 4.0f + 0.32f * 812.0f, by = 135.0f + 0.5f * 218.0f;
        juce::ColourGradient bloom (juce::Colours::white.withAlpha (0.12f), bx, by,
                                    juce::Colours::white.withAlpha (0.0f), bx + 250.0f, by, true);
        g.setGradientFill (bloom);
        g.fillPath (lower);
    }
    g.setColour (juce::Colour (0xff161616)); g.fillRect (7, 132, 806, 3);
    g.setColour (juce::Colour (0xffd2d7d7).withAlpha (0.65f)); g.fillRect (7, 135, 806, 2);

    // Brand block (logo mark = 4 bars at varying heights, per global.css).
    g.setColour (juce::Colours::white);
    {
        const float bx[4] = { 59.0f, 65.0f, 71.0f, 77.0f };
        const float by[4] = { 45.0f, 41.0f, 47.0f, 37.0f };
        for (int i = 0; i < 4; ++i)
            g.fillRoundedRectangle (bx[i], by[i], 4.0f, 20.0f, 2.0f);
    }
    drawText (g, "BASS SEQ", 87, 46, 160, 19.0f, juce::Colours::white, juce::Justification::centredLeft);
    drawText (g, "BASS SEQUENCER", 88, 83, 160, 9.0f, juce::Colours::white, juce::Justification::centredLeft);
    drawText (g, "SOFTWARE BASSLINE SYNTHESIZER", 88, 97, 160, 4.9f, juce::Colour (0xffd8d8d8), juce::Justification::centredLeft);

    // Plates (light strips behind filter/saturation/delay).
    for (const auto& pl : PLATES)
        rr (pl, 16.0f, plate);

    // I/O brackets (open pc-bracket left/right around the jack cluster) — subtle white lines.
    {
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        const float t = 35.0f, b = 119.0f;          // top/bottom of the brackets (top 35, height 84)
        // left bracket ⌐  (x 213..231)
        g.fillRect (213.0f, t, 1.0f, b - t);
        g.fillRect (213.0f, t, 18.0f, 1.0f);
        g.fillRect (213.0f, b - 1.0f, 18.0f, 1.0f);
        // right bracket ¬  (x 276..294)
        g.fillRect (293.0f, t, 1.0f, b - t);
        g.fillRect (276.0f, t, 18.0f, 1.0f);
        g.fillRect (276.0f, b - 1.0f, 18.0f, 1.0f);
    }

    // Jacks: metal ring + dark hole.
    for (const auto& j : JACKS)
    {
        const auto c = juce::Rectangle<float> ((float) j.x, (float) j.y, (float) j.w, (float) j.h);
        const auto centre = c.getCentre();
        const float r = c.getWidth() * 0.5f;

        juce::ColourGradient ring (juce::Colour (0xffaab0b4), centre.x - r * 0.4f, centre.y - r * 0.5f,
                                   juce::Colour (0xff2a2c2e), centre.x + r * 0.3f, centre.y + r * 0.7f, true);
        g.setGradientFill (ring);
        g.fillEllipse (c);

        auto hole = c.reduced (c.getWidth() * 0.27f);
        juce::ColourGradient holeGrad (juce::Colour (0xff1c1c1c), hole.getCentreX(), hole.getY(),
                                       juce::Colour (0xff000000), hole.getCentreX(), hole.getBottom(), false);
        g.setGradientFill (holeGrad);
        g.fillEllipse (hole);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawEllipse (c.reduced (0.7f), 0.8f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (hole, 0.8f);
    }

    // Standalone labels.
    for (const auto& l : LABELS)
        drawText (g, l.t, l.x, l.y, l.w, l.size, l.dim ? textFaint : juce::Colour (0xfff2f2f2));

    // Wave icons (saw on top, square below): centred under the WAVE switch
    // (centre x = 333) and slightly lower, so the switch no longer overlaps them.
    {
        g.setColour (juce::Colour (0xfff2f2f2));
        const float cx = 333.0f;   // = 323 + 20/2 (switch centre)
        juce::Path saw;
        saw.startNewSubPath (cx - 6.0f, 60.0f);
        saw.lineTo (cx - 3.0f, 57.0f); saw.lineTo (cx - 3.0f, 60.0f);
        saw.lineTo (cx,        57.0f); saw.lineTo (cx,        60.0f);
        saw.lineTo (cx + 3.0f, 57.0f); saw.lineTo (cx + 3.0f, 60.0f);
        saw.lineTo (cx + 6.0f, 57.0f);
        g.strokePath (saw, juce::PathStrokeType (0.7f));
        juce::Path sq;
        sq.startNewSubPath (cx - 6.0f, 66.0f);
        sq.lineTo (cx - 6.0f, 64.0f); sq.lineTo (cx - 2.0f, 64.0f);
        sq.lineTo (cx - 2.0f, 66.0f); sq.lineTo (cx + 2.0f, 66.0f);
        sq.lineTo (cx + 2.0f, 64.0f); sq.lineTo (cx + 6.0f, 64.0f);
        sq.lineTo (cx + 6.0f, 66.0f);
        g.strokePath (sq, juce::PathStrokeType (0.7f));
    }

    // Switches: chrome rings with a straight paddle lever that tilts 26° when "on".
    for (const auto& s : SWITCHES)
    {
        const bool on = proc.apvts.getParameter (s.param)->getValue() > 0.5f;
        const auto c = juce::Rectangle<float> ((float) s.x, (float) s.y, (float) s.w, (float) s.h);
        const auto centre = c.getCentre();

        g.setColour (juce::Colour (0xff0a0a0a));
        g.fillEllipse (c.expanded (1.0f));

        // concentric chrome rings (radial: light → grey → dark → light → dark)
        juce::ColourGradient chrome (juce::Colour (0xffececec), centre.x, centre.y,
                                     juce::Colour (0xff222222), centre.x, c.getBottom(), true);
        chrome.addColour (0.26, juce::Colour (0xffececec));
        chrome.addColour (0.45, juce::Colour (0xff777777));
        chrome.addColour (0.62, juce::Colour (0xff151515));
        chrome.addColour (0.80, juce::Colour (0xffd6d6d6));
        g.setGradientFill (chrome);
        g.fillEllipse (c);
        g.setColour (juce::Colour (0xff202020));
        g.drawEllipse (c, 0.8f);

        // paddle lever: upright rounded bar, pivot at the bottom (~85%).
        const float ang   = juce::degreesToRadians (on ? 26.0f : 0.0f);
        const float pivotY = c.getY() + 16.6f;
        juce::Path lever;
        lever.addRoundedRectangle (centre.x - 2.0f, c.getY() + 2.0f, 4.0f, 15.0f, 1.6f);
        lever.applyTransform (juce::AffineTransform::rotation (ang, centre.x, pivotY));
        juce::ColourGradient lg (juce::Colour (0xfff4f4f4), centre.x - 2.0f, 0,
                                 juce::Colour (0xff111111), centre.x + 2.0f, 0, false);
        lg.addColour (0.5, juce::Colour (0xff777777));
        g.setGradientFill (lg);
        g.fillPath (lever);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (lever, juce::PathStrokeType (0.5f));
    }

    // Knob labels — centred exactly under the knob centre (the lx/lw from the source
    // sometimes landed a few px off centre).
    for (const auto& k : KNOBS)
    {
        const float fontSz = (juce::String (k.param) == "MASTER") ? 6.0f : 5.0f;
        const int   cx     = k.x + k.w / 2;
        drawText (g, k.label, cx - 40, k.ly, 80, fontSz, juce::Colour (0xfff2f2f2));
    }

    // Sharp keys: glossy white→blue frame (2px) around a dark core gradient.
    for (const auto& k : SHARPS)
    {
        const bool held = (heldNote == k.midi);
        const auto r = juce::Rectangle<float> ((float) k.x, (float) k.y, (float) k.w, (float) k.h);

        juce::ColourGradient fg (padTop, r.getX(), r.getY(), padBot, r.getX(), r.getBottom(), false);
        g.setGradientFill (fg);
        g.fillRoundedRectangle (r, 5.0f);

        const auto inner = r.reduced (2.0f);
        if (held)
        {
            juce::ColourGradient ig (juce::Colour (0xff223247), inner.getX(), inner.getY(),
                                     juce::Colour (0xff1a2738), inner.getX(), inner.getBottom(), false);
            g.setGradientFill (ig);
        }
        else
        {
            juce::ColourGradient ig (juce::Colour (0xff181818), inner.getX(), inner.getY(),
                                     juce::Colour (0xff171717), inner.getX(), inner.getBottom(), false);
            ig.addColour (0.55, juce::Colour (0xff2b2b2b));
            g.setGradientFill (ig);
        }
        g.fillRoundedRectangle (inner, 3.0f);

        if (held)
        {
            g.setColour (juce::Colour (0xff1a83ff).withAlpha (0.55f));
            g.drawRoundedRectangle (r.reduced (0.6f), 5.0f, 1.6f);
        }
    }

    // Step pads.
    const int  curStep      = proc.getCurrentStep();
    const bool cursorActive = edit.isCursorActive();
    const int  cursorStep   = edit.getEditCursorStep();
    for (const auto& pd : PADS)
    {
        const Step st = proc.getStepForDisplay (pd.step - 1);
        const bool rest    = (st.gateLength == 0 || st.note < 0);
        const bool playing = (curStep == pd.step - 1);
        const bool held    = (heldNote == pd.midi);
        const bool atCursor = (cursorActive && cursorStep == pd.step - 1);

        const auto r = juce::Rectangle<float> ((float) pd.x, (float) pd.y, (float) pd.w, (float) pd.h);

        // glossy frame: white (top) → blue (bottom); rest dims to ~70%.
        const juce::Colour fTop = rest ? padTop.withMultipliedBrightness (0.7f) : padTop;
        const juce::Colour fBot = rest ? padBot.withMultipliedBrightness (0.7f) : padBot;
        juce::ColourGradient fg (fTop, r.getX(), r.getY(), fBot, r.getX(), r.getBottom(), false);
        g.setGradientFill (fg);
        g.fillRoundedRectangle (r, 4.0f);

        // dark core (#333), 2px inset; lighter bluish when pressed.
        juce::Colour inner = held ? juce::Colour (0xff2a3a4e) : padInner;
        if (rest) inner = inner.withMultipliedBrightness (0.7f);
        g.setColour (inner);
        g.fillRoundedRectangle (r.reduced (2.0f), 2.0f);

        // held: blue glow along the edge.
        if (held)
        {
            g.setColour (juce::Colour (0xff1a83ff).withAlpha (0.55f));
            g.drawRoundedRectangle (r.reduced (0.6f), 4.0f, 1.6f);
        }
        // playing step: bright white halo.
        if (playing)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (r.expanded (1.0f), 5.0f, 2.0f);
        }
        // edit cursor: slowly blinking amber border (REC/stepEdit/param-edit).
        if (atCursor && blinkOn && ! playing)
        {
            g.setColour (ledAmber.withAlpha (0.9f));
            g.drawRoundedRectangle (r.expanded (1.0f), 5.0f, 2.0f);
        }

        // step status badges
        if (st.accent)   // amber bar above the pad
        {
            g.setColour (ledAmber);
            g.fillRoundedRectangle (r.getX() + 4.0f, r.getY() - 3.0f, r.getWidth() - 8.0f, 2.0f, 1.0f);
        }
        if (st.slide)    // › bottom-right, light blue
            drawText (g, ">", pd.x, pd.y + pd.h - 12, pd.w - 2, 7.0f, juce::Colour (0xff4fc3f7),
                      juce::Justification::centredRight);
        if (st.ratchet > 1)  // ×N bottom-left, orange
            drawText (g, "x" + juce::String (st.ratchet), pd.x + 2, pd.y + pd.h - 12, pd.w, 6.0f,
                      juce::Colour (0xffff8a4f), juce::Justification::centredLeft);
        if (st.gateLength == 8 && ! rest)   // TIE: connector bar to the next step
        {
            g.setColour (juce::Colour (0xff7fd0ff));
            g.fillRoundedRectangle (r.getRight() - 1.0f, r.getCentreY() - 1.5f, 8.0f, 3.0f, 1.5f);
        }

        // step number below the pad
        drawText (g, juce::String (pd.step), pd.x, pd.y + pd.h + 4, pd.w, 5.0f, juce::Colour (0xff141818).withAlpha (0.7f));
    }

    // Sequencer display (black glass + 7-segment digits).
    g.setColour (juce::Colour (0xff000000)); g.fillRoundedRectangle (668.0f, 150.0f, 126.0f, 53.0f, 4.0f);
    {
        juce::ColourGradient gl (juce::Colour (0xff15171a), 0, 154.0f, juce::Colour (0xff050608), 0, 198.0f, false);
        g.setGradientFill (gl);
        g.fillRoundedRectangle (672.0f, 154.0f, 117.0f, 44.0f, 3.0f);
    }
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (672.0f, 154.0f, 117.0f, 44.0f, 3.0f, 1.0f);
    {
        // Mode-dependent value: tempo / octave (signed) / slot / gate / ratchet / arp.
        drawSegNumber (g, edit.getDisplayValue(), 778.0f, 165.0f, 24.0f, digitCol, juce::Colour (0xff1a1c18));
    }
    // glass reflection
    {
        juce::Path glare;
        glare.startNewSubPath (676, 156); glare.lineTo (724, 156);
        glare.lineTo (700, 196); glare.lineTo (676, 196); glare.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (glare);
    }

    // Button grid (5×3): silver top bevel → dark, with dark insert.
    // LED glow per button follows the modal state machine (EditController::buttonLit).
    for (const auto& b : BUTTONS)
    {
        // mode-edit LEDs blink; flag/transport LEDs give a steady glow.
        const bool lit    = edit.buttonLit (b.id);
        const bool active = lit && (! edit.buttonFlashing (b.id) || blinkOn);
        const auto r = juce::Rectangle<float> ((float) b.x, (float) b.y, (float) b.w, (float) b.h);

        juce::ColourGradient grad (juce::Colour (0xffd2d8da), r.getX(), r.getY(),
                                   juce::Colour (0xff111616), r.getX(), r.getBottom(), false);
        grad.addColour (0.15, juce::Colour (0xffd2d8da));
        grad.addColour (0.19, juce::Colour (0xff576163));
        grad.addColour (0.58, juce::Colour (0xff2a3031));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 4.0f);

        // dark inner insert (::after)
        g.setColour (juce::Colour (0xff2a2f2f));
        g.fillRoundedRectangle (r.getX() + 3.0f, r.getY() + 4.0f, r.getWidth() - 6.0f, r.getHeight() - 7.0f, 2.0f);

        if (active)
        {
            g.setColour (juce::Colour (0xff1a83ff).withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.6f);
        }
        else
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        }

        if (juce::String (b.sym).isNotEmpty())
            drawText (g, b.sym, b.x, b.y + 2, b.w, 9.0f, active ? juce::Colour (0xff8fc6ff) : juce::Colour (0xffd7dddd));
        if (juce::String (b.label).isNotEmpty())
            drawText (g, b.label, b.x - 2, b.y + b.h + 4, b.w + 4, 4.6f, juce::Colour (0xff161b1c));
    }
}

void PadsCabinet::paintOverChildren (juce::Graphics& g)
{
    if (helpVisible)   // after the knob sliders → covers absolutely everything
        drawHelp (g);
}

void PadsCabinet::drawHelp (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.80f));
    const auto card = juce::Rectangle<float> (VIEW_W * 0.5f - 220.0f, 38.0f, 440.0f, 286.0f);
    g.setColour (juce::Colour (0xff1a1e22)); g.fillRoundedRectangle (card, 10.0f);
    g.setColour (juce::Colour (0xff454b52)); g.drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
    g.drawText ("KEYBOARD SHORTCUTS", (int) card.getX() + 18, (int) card.getY() + 12, 400, 20, juce::Justification::centredLeft);

    struct Row { const char* k; const char* d; };
    static const Row rows[] = {
        { "Space",          "Play / Stop" },
        { "R",              "Record / Edit" },
        { "Z   /   X",      "Octave down / up" },
        { "A W S E D F ...","Play notes (A = C, computer-piano)" },
        { "?",              "Show this help" },
        { "Esc / click",    "Close" },
    };
    float y = card.getY() + 46.0f;
    for (const auto& r : rows)
    {
        g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
        g.setColour (juce::Colour (0xff7fd0ff));
        g.drawText (r.k, (int) card.getX() + 24, (int) y, 160, 22, juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.setColour (juce::Colour (0xffd7dde2));
        g.drawText (r.d, (int) card.getX() + 196, (int) y, 230, 22, juce::Justification::centredLeft);
        y += 34.0f;
    }
}

//==============================================================================
bool PadsCabinet::keyPressed (const juce::KeyPress& k)
{
    if (k.getTextCharacter() == '?')                 { if (! helpVisible) { helpVisible = true; repaint(); } return true; }  // show (no flicker on auto-repeat)
    if (helpVisible && k.getKeyCode() == juce::KeyPress::escapeKey) { helpVisible = false; repaint(); return true; }
    return false;   // transport/piano go via keyStateChanged (edge detection, no auto-repeat)
}

bool PadsCabinet::keyStateChanged (bool)
{
    static const std::pair<int, int> PIANO[] = {
        { 'A', 0 }, { 'W', 1 }, { 'S', 2 }, { 'E', 3 }, { 'D', 4 }, { 'F', 5 }, { 'T', 6 },
        { 'G', 7 }, { 'Y', 8 }, { 'H', 9 }, { 'U', 10 }, { 'J', 11 }, { 'K', 12 }
    };
    const double t   = juce::Time::getMillisecondCounterHiRes();
    const int  base  = 48;   // C3 roughly; keyDown applies the octave itself (just like the mouse pads)
    bool used = false;

    auto edge = [this] (int code) -> int {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (code);
        const bool was  = keysDown.count (code) > 0;
        if (down && ! was) { keysDown.insert (code); return +1; }
        if (! down && was) { keysDown.erase (code);  return -1; }
        return 0;
    };

    for (const auto& pk : PIANO)
    {
        const int e = edge (pk.first);
        if (e == +1)
        {
            const auto r = edit.keyDown (-1, base + pk.second, t);
            if (r.sound) { proc.keyboardState.noteOn (1, r.midi, r.accent ? 1.0f : 0.7f); pcNote[pk.first] = r.midi; }
            else         pcNote[pk.first] = -1;
            used = true;
        }
        else if (e == -1)
        {
            auto it = pcNote.find (pk.first);
            if (it != pcNote.end()) { if (it->second >= 0) proc.keyboardState.noteOff (1, it->second, 0.0f); pcNote.erase (it); }
            edit.keyUp (-1, t);
            used = true;
        }
    }

    if (edge (juce::KeyPress::spaceKey) == +1) { edit.gridButtonDown ("play_stop", t); edit.gridButtonUp (t); used = true; }
    if (edge ('R') == +1) { edit.gridButtonDown ("rec_edit", t); edit.gridButtonUp (t); used = true; }
    if (edge ('Z') == +1) { edit.nudgeOctave (-1); used = true; }
    if (edge ('X') == +1) { edit.nudgeOctave (+1); used = true; }

    if (used) repaint();
    return used;
}

//==============================================================================
void PadsCabinet::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();   // clicking the cabinet → computer keyboard works
    if (helpVisible) { helpVisible = false; repaint(); return; }

    const auto pos = e.position;
    const double now = juce::Time::getMillisecondCounterHiRes();
    auto hit = [&pos] (Box b) { return pos.x >= b.x && pos.x < b.x + b.w && pos.y >= b.y && pos.y < b.y + b.h; };

    // Switches (WAVE / SAT_ON / DLY_ON) — direct APVTS toggle.
    for (const auto& s : SWITCHES)
        if (hit ({ s.x, s.y, s.w, s.h }))
        {
            auto* prm = proc.apvts.getParameter (s.param);
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            return;
        }

    // Play the result of a key press; a new note replaces any note still
    // sustained by HOLD (strictly monophonic → one note at a time).
    auto playKey = [this] (const EditController::KeyResult& r)
    {
        if (! r.sound) return;
        if (sustainedNote >= 0 && sustainedNote != r.midi)
            proc.keyboardState.noteOff (1, sustainedNote, 0.0f);
        sustainedNote = -1;
        heldNote = r.midi;
        proc.keyboardState.noteOn (1, r.midi, r.accent ? 1.0f : 0.7f);
    };

    // Step pads (naturals = step 1..16). Sound/write/select via the controller.
    for (const auto& pd : PADS)
        if (hit ({ pd.x, pd.y, pd.w, pd.h }))
        {
            playKey (edit.keyDown (pd.step - 1, pd.midi, now));
            repaint();
            return;
        }

    // Sharp keys (keyboard only, no step number → stepIndex -1).
    for (const auto& k : SHARPS)
        if (hit ({ k.x, k.y, k.w, k.h }))
        {
            playKey (edit.keyDown (-1, k.midi, now));
            repaint();
            return;
        }

    // Grid buttons (5×3) → state machine.
    for (const auto& b : BUTTONS)
        if (hit ({ b.x, b.y, b.w, b.h }))
        {
            edit.gridButtonDown (b.id, now);
            repaint();
            return;
        }
}

void PadsCabinet::mouseUp (const juce::MouseEvent&)
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (heldNote >= 0)
    {
        // Keyboard sustain (HOLD) only applies when stopped; during playback
        // HOLD means arp-hold, and the key must be released normally (→ arp buffer).
        if (edit.isHoldActive() && ! proc.isSequencerPlaying())
            sustainedNote = heldNote;
        else
            proc.keyboardState.noteOff (1, heldNote, 0.0f);
        heldNote = -1;
    }
    edit.keyUp (-1, now);     // clears the held step (REST long-press)
    edit.gridButtonUp (now);  // handles the short action of the released grid button
    repaint();
}

void PadsCabinet::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    edit.tick (now);

    // HOLD turned off while a note was still sustained → release it cleanly.
    if (! edit.isHoldActive() && sustainedNote >= 0)
    {
        proc.keyboardState.noteOff (1, sustainedNote, 0.0f);
        sustainedNote = -1;
    }

    blinkOn = (((int) (now / 400.0)) & 1) == 0;
    const int blink = (edit.isCursorActive() ? (blinkOn ? 1 : 0) : 0);

    const int  step    = proc.getCurrentStep();
    const bool playing = proc.isSequencerPlaying();
    const int  tempo   = (int) proc.apvts.getRawParameterValue ("TEMPO")->load();
    const int  sig     = edit.uiSignature();

    // Real state changes (mode/play/tempo/edit) → full repaint (rare).
    if (playing != lastPlaying || tempo != lastTempo || sig != lastSig || blink != lastBlink)
    {
        lastStep = step; lastPlaying = playing; lastTempo = tempo;
        lastSig = sig; lastBlink = blink;
        repaint();
    }
    // Only the running step changed (the frequent playback case): repaint
    // only the key strip. A full cabinet repaint per step is too heavy and
    // makes the highlight stutter/skip steps; this stays cheap and smooth.
    else if (step != lastStep)
    {
        lastStep = step;
        // x,y,w,h cover all pads + sharps incl. highlight halo, accent bar and step number.
        repaint (24, 152, 620, 198);
    }
}
