#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "Pattern.h"

// (De)serialisation of a Pattern to/from a juce::ValueTree — shared by the
// global bank file (PatternBank) and the plugin state (getStateInformation).
inline juce::ValueTree patternToValueTree (const Pattern& p)
{
    juce::ValueTree t ("PATTERN");
    t.setProperty ("id",        p.id,        nullptr);
    t.setProperty ("tempo",     p.tempo,     nullptr);
    t.setProperty ("length",    p.length,    nullptr);
    t.setProperty ("transpose", p.transpose, nullptr);

    for (int i = 0; i < 16; ++i)
    {
        const Step& s = p.steps[(size_t) i];
        juce::ValueTree st ("STEP");
        st.setProperty ("i",       i,            nullptr);
        st.setProperty ("note",    s.note,       nullptr);
        st.setProperty ("gate",    s.gateLength, nullptr);
        st.setProperty ("accent",  s.accent,     nullptr);
        st.setProperty ("slide",   s.slide,      nullptr);
        st.setProperty ("ratchet", s.ratchet,    nullptr);
        t.appendChild (st, nullptr);
    }
    return t;
}

inline Pattern patternFromValueTree (const juce::ValueTree& t)
{
    Pattern p;
    if (! t.hasType ("PATTERN")) return p;

    p.id        = (int)    t.getProperty ("id",        1);
    p.tempo     = (double) t.getProperty ("tempo",     120.0);
    p.length    = (int)    t.getProperty ("length",    16);
    p.transpose = (int)    t.getProperty ("transpose", 0);

    for (int c = 0; c < t.getNumChildren(); ++c)
    {
        const auto st = t.getChild (c);
        if (! st.hasType ("STEP")) continue;
        const int i = (int) st.getProperty ("i", -1);
        if (i < 0 || i > 15) continue;

        Step s;
        s.note       = (int)  st.getProperty ("note",    -1);
        s.gateLength = (int)  st.getProperty ("gate",    0);
        s.accent     = (bool) st.getProperty ("accent",  false);
        s.slide      = (bool) st.getProperty ("slide",   false);
        s.ratchet    = (int)  st.getProperty ("ratchet", 1);
        p.steps[(size_t) i] = s;
    }
    return sanitizePattern (p);   // clamp every field before it can reach the audio thread
}
