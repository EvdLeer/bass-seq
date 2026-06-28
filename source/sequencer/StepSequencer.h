#pragma once

#include <atomic>
#include <functional>
#include <vector>

#include "Pattern.h"
#include "Arpeggiator.h"

// Sample-accurate step sequencer in the
// sample domain (no JS lookahead needed; processBlock is already sample-accurate).
//
// Deliberately kept juce-free so the timing can be unit-tested in isolation.
// The voice is driven via two callbacks; whether a noteOff occurs before the
// next noteOn determines retrigger vs. slide (Open303 detects that itself).
class StepSequencer
{
public:
    // Live-performance overlays: temporary, NOT stored in the pattern.
    // They sit on top of the step data; set each block from atomics by the processor.
    struct LiveOverrides
    {
        int  transpose = 0;       // extra semitones on all sequencer notes (key − C3)
        bool hold      = false;   // repeat the current step (do not advance)
        bool accent    = false;   // force accent on every step
        bool slide     = false;   // force slide on every step
        int  ratchet   = 0;       // 0 = off, otherwise 1..4 override
        int  gate      = -1;      // -1 = off, otherwise 0..8 override
        bool mute      = false;   // suppress note-ons (timing keeps running)
    };

    std::function<void (int note, bool accent)> onNoteOn;   // voice.noteOn (accent → velocity)
    std::function<void ()>                      onNoteOff;  // voice.noteOff (release)
    std::function<void ()>                      onPatternWrap;  // on wrap back to step 0 (audio thread)

    void setLive (const LiveOverrides& l);                  // set by the processor (audio thread)

    // Arp: when active the arp replaces the pattern as the note source on
    // the same step grid. The processor owns the Arpeggiator object and feeds it.
    void setArp (Arpeggiator* a) { arp = a; }
    void setArpParams (bool active, int gate, bool accent);

    void prepare (double sampleRate);
    void setPattern (const Pattern* p) { pattern = p; }
    void setTempo (double bpm);

    void start();
    void stop();
    bool isPlaying() const   { return running; }
    int  getCurrentStep() const { return currentStepUi.load(); }

    void processSample();   // call once per output sample

private:
    void scheduleStep (int stepIdx);
    void scheduleGateOff (int gate, double whenAbs, double stepDurSamples, bool nextIsRest);
    void firePending (double nowAbs);
    void advanceStep();
    int  resolveNote (int rawNote) const;   // + transpose, range-check → -1 if outside 0..127

    struct Pending { double timeAbs; bool noteOff; int note; bool accent; };

    const Pattern* pattern        = nullptr;
    double sampleRate             = 44100.0;
    double bpm                    = 120.0;
    double samplesPerStep         = 5512.5;

    LiveOverrides live;

    Arpeggiator* arp        = nullptr;
    bool   arpActive        = false;
    int    arpGate          = 8;
    bool   arpAccent        = false;

    bool   running                = false;
    int    currentStep            = 0;
    bool   prevWasSlide           = false;
    double globalSample           = 0.0;    // monotonic sample counter
    double samplesToNextStep      = 0.0;

    std::vector<Pending> pending;
    std::atomic<int> currentStepUi { -1 };
};
