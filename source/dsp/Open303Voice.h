#pragma once

#include <memory>

// Opaque forward-declaration so the heavy rosic headers (and their global
// `using namespace std;`) do NOT leak into the rest of the plugin. The real
// include happens only in Open303Voice.cpp.
namespace rosic { class Open303; }

// Thin wrapper around rosic::Open303 (the TB-303 emulation). Mirrors the
// knob->parameter mappings. Monophonic; note priority lives in
// VoiceAllocator, not here.
class Open303Voice
{
public:
    Open303Voice();
    ~Open303Voice();

    void prepare (double sampleRate);

    // Knob handlers (x in 0..1, unless otherwise noted).
    void setWaveformSquare (bool square);          // false = saw, true = square
    void setCutoffKnob (float x);                  // expMap -> 15..12000 Hz
    void setResonanceKnob (float x);               // x100 (0..100%)
    void setEnvModKnob (float x);                  // x100 (0..100%)
    void setDecayKnob (float x);                   // expMap -> 0.03..2.5 s (filter + amp)
    void setAccentKnob (float x);                  // x100 (0..100%)
    void setPitchOffsetSemitones (float semis);    // -7..+7, rounded to whole semitones

    // Voice events (note priority handled externally).
    void noteOn (int midi, bool accent);
    void noteOff();
    void allNotesOff();

    float getSample();   // one mono sample

private:
    std::unique_ptr<rosic::Open303> core;
    int  currentNote = -1;
    int  pitchOffset = 0;
    bool sounding    = false;   // is a note currently sounding? decides trigger vs. slide
};
