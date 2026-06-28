#include "StepSequencer.h"

#include <cmath>   // std::isfinite

namespace
{
    int clampInt (int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
    double clampD (double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
}

void StepSequencer::prepare (double sr)
{
    sampleRate = sr;
    samplesPerStep = sampleRate * (60.0 / bpm) / 4.0;   // 1/16 note
    pending.reserve (32);
}

void StepSequencer::setTempo (double newBpm)
{
    if (! std::isfinite (newBpm)) newBpm = 120.0;
    bpm = clampD (newBpm, 20.0, 300.0);
    samplesPerStep = sampleRate * (60.0 / bpm) / 4.0;
}

void StepSequencer::setArpParams (bool active, int gate, bool accent)
{
    if (active != arpActive)   // switching to/from arp -> release any held note, clear scheduling
    {
        if (onNoteOff) onNoteOff();
        prevWasSlide = false;
        pending.clear();
    }
    arpActive = active;
    arpGate   = clampInt (gate, 1, 8);
    arpAccent = accent;
}

void StepSequencer::setLive (const LiveOverrides& l)
{
    // Live mute just enabled -> cleanly release any still-sounding note (a TIE or a
    // slide without a scheduled note-off) and cancel scheduled events, so mute is
    // truly silent rather than just suppressing new note-ons.
    if (l.mute && ! live.mute)
    {
        if (onNoteOff) onNoteOff();
        prevWasSlide = false;
        pending.clear();
    }
    live = l;
}

void StepSequencer::start()
{
    running           = true;
    currentStep       = 0;
    prevWasSlide      = false;
    globalSample      = 0.0;
    samplesToNextStep = 0.0;        // -> step 0 fires immediately
    pending.clear();
    currentStepUi.store (0);
}

void StepSequencer::stop()
{
    running      = false;
    prevWasSlide = false;
    pending.clear();
    if (onNoteOff) onNoteOff();      // cleanly release any held note
    currentStepUi.store (-1);
}

int StepSequencer::resolveNote (int rawNote) const
{
    if (rawNote < 0) return -1;
    const int n = rawNote + (pattern != nullptr ? pattern->transpose : 0) + live.transpose;
    return (n < 0 || n > 127) ? -1 : n;   // out of range -> step stays silent (live transpose)
}

void StepSequencer::advanceStep()
{
    if (live.hold)   // live HOLD: current step keeps repeating, cursor does not advance
        return;

    const int len = clampInt (pattern != nullptr ? pattern->length : 16, 1, 16);
    currentStep = (currentStep + 1) % len;
    if (currentStep == 0 && onPatternWrap)   // wrapped back -> pattern boundary
        onPatternWrap();
}

void StepSequencer::processSample()
{
    if (running && pattern != nullptr)
    {
        if (samplesToNextStep <= 0.0)
        {
            currentStepUi.store (currentStep);
            scheduleStep (currentStep);     // immediate emits + schedule future events
            advanceStep();
            samplesToNextStep += samplesPerStep;
        }
        firePending (globalSample);         // events due now (incl. ratchet i=0)
        samplesToNextStep -= 1.0;
    }
    globalSample += 1.0;
}

void StepSequencer::scheduleStep (int stepIdx)
{
    const Step& step = pattern->steps[(size_t) clampInt (stepIdx, 0, 15)];
    const double when    = globalSample;       // step start (absolute sample)
    const double stepDur = samplesPerStep;

    // ---- Live MUTE: emit nothing, but keep the scheduler running (timing in sync) ----
    if (live.mute) { prevWasSlide = false; return; }

    // ---- ARP: replaces the pattern as note source on the same grid -----------
    if (arpActive && arp != nullptr)
    {
        prevWasSlide = false;
        if (! arp->hasNotes()) return;                 // no chord -> silent step
        const int an = arp->nextNote();
        if (an < 0) return;
        // Deliberately no resolveNote(): the arp REPLACES the sequencer, so the
        // STORED pattern transpose does not apply; only the live transpose does.
        // Octave extensions (+/-12) may fall outside 0..127 -> that step stays silent.
        const int n = an + live.transpose;
        if (n < 0 || n > 127) return;                  // out of MIDI range -> silent
        if (onNoteOn) onNoteOn (n, arpAccent || live.accent);
        scheduleGateOff (arpGate, when, stepDur, true);  // arp note closes within the step
        return;
    }

    // ---- Effective step parameters (live overrides sit on top) -----
    const int  eGate    = clampInt ((live.gate >= 0) ? live.gate : step.gateLength, 0, 8);
    const bool eAccent  = step.accent || live.accent;
    const bool eSlide   = step.slide  || live.slide;
    const int  eRatchet = clampInt ((live.ratchet > 0) ? live.ratchet : step.ratchet, 1, 4);  // bound the ratchet loop

    // REST
    if (eGate == 0 || step.note < 0) { prevWasSlide = false; return; }

    const int note = resolveNote (step.note);
    if (note < 0) { prevWasSlide = false; return; }

    const int len = clampInt (pattern->length, 1, 16);
    const Step& next = pattern->steps[(size_t) ((stepIdx + 1) % len)];
    const int  nextGate   = (live.gate >= 0) ? live.gate : next.gateLength;
    const bool nextIsRest = (nextGate == 0 || next.note < 0);

    // ---- SLIDE -------------------------------------------------------------
    if (eSlide && ! nextIsRest)
    {
        const int nextNote = resolveNote (next.note);
        if (nextNote >= 0)
        {
            if (onNoteOn)
                onNoteOn (note, prevWasSlide ? false : eAccent);  // landing = legato, no accent
            // glide to the next note: noteOn without noteOff -> Open303 slews.
            if (onNoteOn) onNoteOn (nextNote, false);
            // no note-off: the gate stays open past the next step.
            prevWasSlide = true;
            return;
        }
        // next note out of range -> fall back to the normal path
    }

    // ---- Landing of a previous slide (legato, no retrigger) -------------
    if (prevWasSlide)
    {
        if (onNoteOn) onNoteOn (note, false);
        scheduleGateOff (eGate, when, stepDur, nextIsRest);
        prevWasSlide = false;
        return;
    }

    // ---- RATCHET (ignored on slide/tie/rest) ----------------------------
    if (eRatchet > 1 && eGate != 8)
    {
        const double sub     = stepDur / eRatchet;
        const double subGate = sub * 0.5;
        for (int i = 0; i < eRatchet; ++i)
        {
            const double subOn = when + i * sub;
            pending.push_back ({ subOn,           false, note, eAccent });  // sub-noteOn
            pending.push_back ({ subOn + subGate, true,  -1,   false });    // sub-noteOff
        }
        prevWasSlide = false;
        return;
    }

    // ---- Normal note / TIE ------------------------------------------------
    if (onNoteOn) onNoteOn (note, eAccent);
    scheduleGateOff (eGate, when, stepDur, nextIsRest);
    prevWasSlide = false;
}

void StepSequencer::scheduleGateOff (int gate, double whenAbs, double stepDur, bool nextIsRest)
{
    if (gate == 8)   // TIE: only close at the step boundary if the next step is a rest
    {
        if (nextIsRest)
            pending.push_back ({ whenAbs + stepDur, true, -1, false });
        return;
    }
    const double off = whenAbs + (gate / 8.0) * stepDur;
    pending.push_back ({ off, true, -1, false });
}

void StepSequencer::firePending (double nowAbs)
{
    for (size_t i = 0; i < pending.size(); )
    {
        if (pending[i].timeAbs <= nowAbs)
        {
            if (pending[i].noteOff) { if (onNoteOff) onNoteOff(); }
            else                    { if (onNoteOn)  onNoteOn (pending[i].note, pending[i].accent); }
            pending.erase (pending.begin() + (long) i);
        }
        else
        {
            ++i;
        }
    }
}
