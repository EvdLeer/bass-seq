#pragma once

#include <array>

// Data model for a single 16-step pattern.
struct Step
{
    int  note       = -1;      // MIDI note; -1 = empty (rest), like TS `null`
    int  gateLength = 0;       // 0 = rest, 1..7 = fraction of the step, 8 = TIE
    bool accent     = false;
    bool slide      = false;
    int  ratchet    = 1;       // 1..4

    bool operator== (const Step& o) const
    {
        return note == o.note && gateLength == o.gateLength && accent == o.accent
            && slide == o.slide && ratchet == o.ratchet;
    }
    bool operator!= (const Step& o) const { return ! (*this == o); }
};

struct Pattern
{
    int    id        = 1;
    double tempo     = 120.0;  // 40..240 BPM
    int    length    = 16;     // active number of steps (1..16); always 16 stored
    int    transpose = 0;      // -12..+12 semitones
    std::array<Step, 16> steps {};
};

// --- Input sanitisation -------------------------------------------------------
// Patterns can arrive from persisted state (plugin session, global bank file) that
// a third party could have tampered with. Clamp every field into its valid domain
// on the way in, so out-of-range values can never reach the audio thread — e.g. a
// bogus `length` driving an out-of-bounds steps[] read, or a huge `ratchet`
// scheduling millions of events. POD/juce-free, like the rest of this header.
inline int clampPatternInt (int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

inline Step sanitizeStep (Step s)
{
    s.note       = (s.note >= 0 && s.note <= 127) ? s.note : -1;
    s.gateLength = clampPatternInt (s.gateLength, 0, 8);
    s.ratchet    = clampPatternInt (s.ratchet, 1, 4);
    // accent/slide are bool and cannot be out of range.
    return s;
}

inline Pattern sanitizePattern (Pattern p)
{
    p.id        = clampPatternInt (p.id, 1, 128);
    p.tempo     = (p.tempo >= 40.0 && p.tempo <= 240.0) ? p.tempo : 120.0;  // out-of-range / NaN -> safe default
    p.length    = clampPatternInt (p.length, 1, 16);
    p.transpose = clampPatternInt (p.transpose, -12, 12);
    for (auto& s : p.steps)
        s = sanitizeStep (s);
    return p;
}

// Placeholder starting pattern: ascending natural notes B1..C4 (matches the
// step<->key mapping), gate = 4, without accent/slide/ratchet — a neutral,
// playable start until there is pattern memory. No step decorations, so no
// stray accent dashes/slide arrow/xN in the UI.
inline Pattern makeTestPattern()
{
    static const int naturals[16] =
        { 35, 36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55, 57, 59, 60 };

    Pattern p;
    for (size_t i = 0; i < 16; ++i)
    {
        p.steps[i].note       = naturals[i];
        p.steps[i].gateLength = 4;
        p.steps[i].ratchet    = 1;
    }
    return p;
}
