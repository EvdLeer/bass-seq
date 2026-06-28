#pragma once

#include "Pattern.h"

// Edit commands the UI thread sends to the audio thread. The audio thread is the
// ONLY writer of the active Pattern (the sequencer reads it per sample); the UI
// never mutates it directly. This avoids torn reads → no wrong or stuck notes
// while editing. Deliberately juce-free POD, like Pattern.h.
struct PatternCommand
{
    enum class Type
    {
        writeNote,     // step ← { note, gateLength=value (inherited), rest default }  (REC write)
        setNote,       // step.note = note                                          (stepEdit)
        setGate,       // step.gateLength = value (0..8)
        setRatchet,    // step.ratchet = value (1..4)
        toggleAccent,  // step.accent = !step.accent
        toggleSlide,   // step.slide  = !step.slide
        setRest,       // step.gateLength = 0
        clearStep,     // step ← default Step
        initPattern,   // all steps ← default Step
        setTranspose   // pattern.transpose = value
    };

    Type type  = Type::writeNote;
    int  step  = 0;     // 0..15
    int  note  = -1;    // MIDI note for write/setNote
    int  value = 0;     // generic payload (gate/ratchet/transpose)
};

inline void applyPatternCommand (Pattern& p, const PatternCommand& c)
{
    auto clampI = [] (int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); };
    const size_t s = (size_t) clampI (c.step, 0, 15);

    switch (c.type)
    {
        case PatternCommand::Type::writeNote:
        {
            Step st;
            st.note       = (c.note >= 0 && c.note <= 127) ? c.note : -1;
            st.gateLength = clampI (c.value, 1, 8);   // inherited gate; at least 1 (otherwise it would be a rest)
            st.accent     = false;
            st.slide      = false;
            st.ratchet    = 1;
            p.steps[s] = st;
            break;
        }
        case PatternCommand::Type::setNote:
            p.steps[s].note = (c.note >= 0 && c.note <= 127) ? c.note : -1;
            break;
        case PatternCommand::Type::setGate:
            p.steps[s].gateLength = clampI (c.value, 0, 8);
            break;
        case PatternCommand::Type::setRatchet:
            p.steps[s].ratchet = clampI (c.value, 1, 4);
            break;
        case PatternCommand::Type::toggleAccent:
            p.steps[s].accent = ! p.steps[s].accent;
            break;
        case PatternCommand::Type::toggleSlide:
            p.steps[s].slide = ! p.steps[s].slide;
            break;
        case PatternCommand::Type::setRest:
            p.steps[s].gateLength = 0;
            break;
        case PatternCommand::Type::clearStep:
            p.steps[s] = Step{};
            break;
        case PatternCommand::Type::initPattern:
            for (auto& st : p.steps) st = Step{};
            break;
        case PatternCommand::Type::setTranspose:
            p.transpose = clampI (c.value, -12, 12);
            break;
    }
}
