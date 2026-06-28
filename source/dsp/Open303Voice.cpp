#include "Open303Voice.h"

// The rosic source relies on standard headers being pulled in transitively (as
// emscripten's libc++ used to provide). Include them explicitly before the
// rosic header, because libstdc++ does not (memcpy/INT_MAX/NULL/...).
#include <cstring>
#include <cstdlib>
#include <climits>
#include <cfloat>
#include <cstddef>
#include <cmath>

#include <rosic_Open303.h>   // angle brackets → via the SYSTEM include path (warnings suppressed)

namespace
{
    // Knob→Open303 parameter mappings.
    constexpr double CUTOFF_MIN_HZ = 15.0;
    constexpr double CUTOFF_MAX_HZ = 12000.0;
    constexpr double DECAY_MIN_S   = 0.03;
    constexpr double DECAY_MAX_S   = 2.5;

    double expMap (double x, double lo, double hi)
    {
        x = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
        return lo * std::pow (hi / lo, x);
    }
}

Open303Voice::Open303Voice()
    : core (std::make_unique<rosic::Open303>()) {}

Open303Voice::~Open303Voice() = default;

void Open303Voice::prepare (double sampleRate)
{
    core->setSampleRate (sampleRate);
    core->setTuning (440.0);
}

void Open303Voice::setWaveformSquare (bool square) { core->setWaveform (square ? 1.0 : 0.0); }

void Open303Voice::setCutoffKnob (float x)    { core->setCutoff (expMap (x, CUTOFF_MIN_HZ, CUTOFF_MAX_HZ)); }
void Open303Voice::setResonanceKnob (float x) { core->setResonance (x * 100.0); }
void Open303Voice::setEnvModKnob (float x)    { core->setEnvMod (x * 100.0); }

void Open303Voice::setDecayKnob (float x)
{
    const double ms = expMap (x, DECAY_MIN_S, DECAY_MAX_S) * 1000.0;
    core->setDecay (ms);      // filter-envelope decay
    core->setAmpDecay (ms);   // amp-envelope decay (so note length tracks along)
}

void Open303Voice::setAccentKnob (float x) { core->setAccent (x * 100.0); }

void Open303Voice::setPitchOffsetSemitones (float semis)
{
    pitchOffset = (int) std::lround (semis);
}

void Open303Voice::noteOn (int midi, bool accent)
{
    int n = midi + pitchOffset;
    if (n < 0)   n = 0;
    if (n > 127) n = 127;

    // Strict monophony: if nothing is sounding, first fully clear the noteList
    // so Open303 retriggers; if something is already sounding, Open303 glides
    // (slide/legato). Whether a release occurred in between thus decides trigger vs. slide.
    if (! sounding)
        core->allNotesOff();

    core->noteOn (n, accent ? 127 : 100, 0.0);   // velocity 127 = accent
    sounding    = true;
    currentNote = n;
}

void Open303Voice::noteOff()
{
    core->allNotesOff();     // full release — never leave notes hanging in the list
    sounding    = false;
    currentNote = -1;
}

void Open303Voice::allNotesOff()
{
    core->allNotesOff();
    sounding    = false;
    currentNote = -1;
}

float Open303Voice::getSample()
{
    return (float) core->getSample();
}
