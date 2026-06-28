#include "EditController.h"

namespace
{
    int clampI (int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
}

//==============================================================================
void EditController::post (PatternCommand::Type t, int step, int note, int value)
{
    PatternCommand c;
    c.type = t; c.step = step; c.note = note; c.value = value;
    proc.postPatternCommand (c);
}

void EditController::advanceCursor (int dir)
{
    const int len = juce::jmax (1, proc.getPatternLength());
    editCursorStep = ((editCursorStep + dir) % len + len) % len;
    refreshEditValue();
}

void EditController::refreshEditValue()
{
    const Step s = cursorStep();
    editValue = (mode == Mode::ratchetEdit) ? s.ratchet : s.gateLength;
}

void EditController::nudgeOctave (int dir)
{
    octaveShift = clampI (octaveShift + dir, -3, 3);
}

int EditController::inheritedGate() const
{
    // A newly written step inherits the gate length from the previous step.
    // Read it directly; if the previous step is a rest (gate 0), fall back to the
    // last deliberate gate so inheritance doesn't accidentally pass on a rest.
    const int len  = juce::jmax (1, proc.getPatternLength());
    const int prev = ((editCursorStep - 1) % len + len) % len;
    const int pg   = proc.getStepForDisplay (prev).gateLength;
    return pg > 0 ? pg : lastWrittenGate;
}

bool EditController::editingStep() const
{
    return recordActive || mode == Mode::stepEdit
        || mode == Mode::gateLengthEdit || mode == Mode::ratchetEdit;
}

bool EditController::isCursorActive() const
{
    return recordActive || mode == Mode::stepEdit
        || mode == Mode::gateLengthEdit || mode == Mode::ratchetEdit;
}

//==============================================================================
EditController::KeyResult EditController::keyDown (int stepIndex, int baseMidi, double nowMs)
{
    touch (nowMs);
    const int playMidi = clampI (baseMidi + 12 * octaveShift, 0, 127);

    // 1) Armed step-select (REC/EDIT long-pressed) + natural -> stepEdit.
    if (armStepSelect && stepIndex >= 0)
    {
        armStepSelect  = false;
        mode           = Mode::stepEdit;
        editCursorStep = stepIndex;
        refreshEditValue();
        return {};   // select only, no sound
    }

    // 2) REC mode -> write note to cursor (gate inherits from previous step), advance.
    if (recordActive)
    {
        const int gate = inheritedGate();
        lastWrittenGate = gate;
        post (PatternCommand::Type::writeNote, editCursorStep, playMidi, gate);
        advanceCursor (+1);
        return { true, playMidi, accentLive };   // sounds as feedback
    }

    // 3) stepEdit -> set note on the cursor step (cursor does NOT advance).
    if (mode == Mode::stepEdit)
    {
        post (PatternCommand::Type::setNote, editCursorStep, playMidi, 0);
        return { true, playMidi, accentLive };
    }

    // 4) During playback WITHOUT arp -> live transpose: the key shifts the
    //    sequencer by (key - C3/MIDI 48) instead of a separate note. Uses the
    //    raw key MIDI literally; the keyboard octave does NOT factor into the offset.
    //    (With arp active it falls through to (5): the key goes via keyboardState into
    //     the chord buffer.)
    if (proc.isSequencerPlaying() && ! arpActive)
    {
        proc.setLiveTranspose (baseMidi - 48);
        liveTransposeOn = true;
        return {};   // no separate sound
    }

    // 5) keyboard/default -> play; remember for long-press REST toggle during playback.
    heldStep     = stepIndex;
    heldStepTime = nowMs;
    heldStepLong = false;
    return { true, playMidi, accentLive };
}

void EditController::keyUp (int /*stepIndex*/, double nowMs)
{
    touch (nowMs);
    heldStep = -2;
    if (liveTransposeOn)   // key released -> live transpose back to 0
    {
        proc.setLiveTranspose (0);
        liveTransposeOn = false;
    }
}

//==============================================================================
void EditController::gridButtonDown (const juce::String& id, double nowMs)
{
    touch (nowMs);
    armStepSelect  = false;   // a new grid press cancels a pending step-select arm
    heldButton     = id;
    heldButtonTime = nowMs;
    heldButtonLong = false;

    // Buttons that react directly on 'down' (responsive + repeat).
    if (id == "up" || id == "down")
    {
        heldArrow       = (id == "up") ? +1 : -1;
        nextArrowRepeat = nowMs + kArrowDelayMs;
        applyArrow (heldArrow, nowMs);
    }
    else if (id == "play_stop")
    {
        togglePlay();
    }
}

void EditController::gridButtonUp (double nowMs)
{
    touch (nowMs);
    const juce::String id = heldButton;
    heldArrow = 0;

    if (! liveButton.isEmpty() && liveButton == id)
    {
        endLive();   // live override ends as soon as the button is released
    }
    else if (id == "up" || id == "down")
    {
        // Releasing the up/down arrow in pattern-select -> the chosen slot only loads now
        // (release -> the pattern is loaded). While scrolling, only the slot
        // number in the display changes.
        if (mode == Mode::patternSelect)
            proc.loadSlot (selectedSlot);
    }
    else if (id.isNotEmpty() && ! heldButtonLong)
    {
        shortAction (id, nowMs);
    }

    heldButton     = {};
    heldButtonLong = false;
}

//==============================================================================
void EditController::shortAction (const juce::String& id, double nowMs)
{
    // 'up'/'down'/'play_stop' are already handled on 'down'.
    if (id == "up" || id == "down" || id == "play_stop")
        return;

    if (id == "oct")
    {
        mode = (mode == Mode::octaveEdit) ? Mode::keyboard : Mode::octaveEdit;
    }
    else if (id == "rec_edit")
    {
        if (mode == Mode::stepEdit)
        {
            mode = Mode::keyboard;
        }
        else
        {
            recordActive = ! recordActive;
            if (recordActive) { editCursorStep = 0; mode = Mode::keyboard; refreshEditValue(); }
        }
    }
    else if (id == "tempo_tap")
    {
        registerTap (nowMs);
        mode = Mode::tempoEdit;
    }
    else if (id == "pattern")
    {
        selectedSlot = proc.getCurrentSlot();
        mode = Mode::patternSelect;
    }
    else if (id == "save")
    {
        if (mode == Mode::patternSave)
        {
            proc.saveWorkingToSlot (selectedSlot);   // 2nd press = confirm -> save
            mode = Mode::keyboard;
        }
        else
        {
            selectedSlot = proc.getCurrentSlot();
            mode = Mode::patternSave;
        }
    }
    else if (id == "clear_rest")
    {
        if (recordActive)             // write REST + advance
        {
            post (PatternCommand::Type::setRest, editCursorStep, -1, 0);
            advanceCursor (+1);
        }
        else if (mode == Mode::stepEdit)   // toggle REST on the selected step (no advance)
        {
            if (cursorStep().gateLength == 0)
                post (PatternCommand::Type::setGate, editCursorStep, -1, inheritedGate());
            else
                post (PatternCommand::Type::setRest, editCursorStep, -1, 0);
        }
        // (live mute: by holding CLEAR/REST)
    }
    else if (id == "gate_length")
    {
        mode = (mode == Mode::gateLengthEdit) ? Mode::keyboard : Mode::gateLengthEdit;
        refreshEditValue();
    }
    else if (id == "ratchet")
    {
        mode = (mode == Mode::ratchetEdit) ? Mode::keyboard : Mode::ratchetEdit;
        refreshEditValue();
    }
    else if (id == "accent")
    {
        if (arpActive)                // ARP active -> toggle global arp accent
        {
            arpAccentOn = ! arpAccentOn;
            proc.setArpAccent (arpAccentOn);
        }
        else if (editingStep())
            post (PatternCommand::Type::toggleAccent, editCursorStep, -1, 0);
        else
            accentLive = ! accentLive;
    }
    else if (id == "slide")
    {
        if (editingStep())
            post (PatternCommand::Type::toggleSlide, editCursorStep, -1, 0);
        // (live slide: by holding SLIDE)
    }
    else if (id == "arp")
    {
        arpActive = ! arpActive;
        proc.setArpActive (arpActive);
    }
    else if (id == "hold")
    {
        holdActive = ! holdActive;    // keyboard sustain and (with arp) arp hold after release
        proc.setArpHold (holdActive);
    }
}

void EditController::applyArrow (int dir, double nowMs)
{
    touch (nowMs);

    // Active live override -> up/down adjust the live ratchet/gate value.
    if (liveButton == "ratchet")
    {
        liveRatchetVal = clampI (liveRatchetVal + dir, 1, 4);
        proc.setLiveRatchet (liveRatchetVal);
        return;
    }
    if (liveButton == "gate_length")
    {
        liveGateVal = clampI (liveGateVal + dir, 0, 8);
        proc.setLiveGate (liveGateVal);
        return;
    }

    // With ARP active in keyboard mode, up/down scroll through the arp mode (1..8).
    if (mode == Mode::keyboard && arpActive)
    {
        arpMode = clampI (arpMode + dir, 1, 8);
        proc.setArpMode (arpMode);
        return;
    }

    // ARP active + GATE LENGTH mode -> adjust the ARP gate instead of the step gate.
    if (mode == Mode::gateLengthEdit && arpActive)
    {
        arpGateVal = clampI (arpGateVal + dir, 1, 8);
        proc.setArpGate (arpGateVal);
        return;
    }

    switch (mode)
    {
        case Mode::keyboard:
        case Mode::stepEdit:
            advanceCursor (dir);
            break;
        case Mode::tempoEdit:
            nudgeTempo (dir);
            break;
        case Mode::octaveEdit:
            octaveShift = clampI (octaveShift + dir, -3, 3);
            break;
        case Mode::patternSelect:
            selectedSlot = clampI (selectedSlot + dir, 1, 128);   // select only; loading happens on release
            break;
        case Mode::patternSave:
            selectedSlot = clampI (selectedSlot + dir, 1, 128);   // only choose the target slot
            break;
        case Mode::gateLengthEdit:
            editValue = clampI (editValue + dir, 0, 8);
            post (PatternCommand::Type::setGate, editCursorStep, -1, editValue);
            if (editValue > 0) lastWrittenGate = editValue;
            break;
        case Mode::ratchetEdit:
            editValue = clampI (editValue + dir, 1, 4);
            post (PatternCommand::Type::setRatchet, editCursorStep, -1, editValue);
            break;
    }
}

void EditController::maybeFireLong (double nowMs)
{
    // Grid button long-press.
    if (heldButton.isNotEmpty() && ! heldButtonLong)
    {
        const double held = nowMs - heldButtonTime;
        if (heldButton == "rec_edit" && held >= kArmPressMs)
        {
            armStepSelect  = true;     // next step click -> stepEdit
            armTime        = nowMs;
            heldButtonLong = true;
            touch (nowMs);
        }
        else if (held >= kLongPressMs)
        {
            if (heldButton == "clear_rest")
            {
                if (recordActive)
                    post (PatternCommand::Type::initPattern, 0, -1, 0);   // initialize pattern
            }
            else if (heldButton == "tempo_tap" || heldButton == "save")
            {
                mode = Mode::keyboard;   // leave tempo edit / cancel save
            }
            else if (heldButton == "pattern")
            {
                proc.revertToCurrentSlot();   // reload current slot (revert unsaved edits)
            }
            heldButtonLong = true;
            touch (nowMs);
        }
    }

    // up/down fast repeat (after 300 ms, ~20 Hz).
    if (heldArrow != 0 && nowMs >= nextArrowRepeat)
    {
        applyArrow (heldArrow, nowMs);
        nextArrowRepeat = nowMs + kArrowRepeatMs;
    }

    // Step button >=2 s during playback -> toggle REST (real-time pattern edit).
    if (heldStep >= 0 && ! heldStepLong && proc.isSequencerPlaying()
        && nowMs - heldStepTime >= kLongPressMs)
    {
        const Step s = proc.getStepForDisplay (heldStep);
        post (s.gateLength == 0 ? PatternCommand::Type::setGate    // from rest back to sounding
                                : PatternCommand::Type::setRest,
              heldStep, -1, lastWrittenGate);
        heldStepLong = true;
        touch (nowMs);
    }
}

void EditController::tick (double nowMs)
{
    maybeFireLong (nowMs);

    // Activate live override: hold a live button > 250 ms during playback.
    // (CLEAR/REST only acts as live mute when NOT recording -- there it's the 2s init.)
    if (liveButton.isEmpty() && heldButton.isNotEmpty() && ! heldButtonLong
        && proc.isSequencerPlaying() && isLiveButton (heldButton)
        && (heldButton != "clear_rest" || ! recordActive)
        && nowMs - heldButtonTime >= kLiveHoldMs)
    {
        startLive (heldButton, nowMs);
    }

    // Does playback stop while a live button is still held? Then lift the override
    // immediately, otherwise it stays set in the processor and restarts with it.
    if (! proc.isSequencerPlaying())
    {
        if (! liveButton.isEmpty()) endLive();
        if (liveTransposeOn) { proc.setLiveTranspose (0); liveTransposeOn = false; }
    }

    // Armed step-select expires on its own, so a lingering arm can't swallow a
    // later pad click.
    if (armStepSelect && nowMs - armTime > kArmTimeoutMs)
        armStepSelect = false;

    // Temporary edit modes fall back to keyboard after 5 s of inactivity.
    const bool temporary =
        mode == Mode::tempoEdit || mode == Mode::octaveEdit
     || mode == Mode::patternSelect || mode == Mode::patternSave
     || mode == Mode::gateLengthEdit || mode == Mode::ratchetEdit;

    if (temporary && nowMs - lastInteraction > kModeTimeoutMs)
        mode = Mode::keyboard;
}

//==============================================================================
void EditController::togglePlay()
{
    if (auto* prm = proc.apvts.getParameter ("PLAY"))
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
}

void EditController::nudgeTempo (int dir)
{
    if (auto* prm = proc.apvts.getParameter ("TEMPO"))
    {
        const float cur = proc.apvts.getRawParameterValue ("TEMPO")->load();
        const float nv  = (float) clampI ((int) std::lround (cur) + dir, 40, 240);
        prm->setValueNotifyingHost (prm->convertTo0to1 (nv));
    }
}

void EditController::registerTap (double nowMs)
{
    if (! tapTimes.empty() && nowMs - tapTimes.back() > 2000.0)
        tapTimes.clear();
    tapTimes.push_back (nowMs);
    if (tapTimes.size() > 4) tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() >= 2)
    {
        double sum = 0.0;
        for (size_t i = 1; i < tapTimes.size(); ++i) sum += tapTimes[i] - tapTimes[i - 1];
        const double avg = sum / (double) (tapTimes.size() - 1);
        if (avg > 1.0)
            if (auto* prm = proc.apvts.getParameter ("TEMPO"))
                prm->setValueNotifyingHost (
                    prm->convertTo0to1 ((float) juce::jlimit (40.0, 240.0, 60000.0 / avg)));
    }
}

//==============================================================================
bool EditController::isLiveButton (const juce::String& id) const
{
    return id == "accent" || id == "slide" || id == "hold"
        || id == "ratchet" || id == "gate_length" || id == "clear_rest";
}

void EditController::startLive (const juce::String& id, double nowMs)
{
    liveButton = id;
    if      (id == "accent")      proc.setLiveAccent (true);
    else if (id == "slide")       proc.setLiveSlide (true);
    else if (id == "hold")        proc.setLiveHold (true);
    else if (id == "clear_rest")  proc.setLiveMute (true);
    else if (id == "ratchet")     proc.setLiveRatchet (liveRatchetVal);
    else if (id == "gate_length") proc.setLiveGate (liveGateVal);
    touch (nowMs);
}

void EditController::endLive()
{
    if      (liveButton == "accent")      proc.setLiveAccent (false);
    else if (liveButton == "slide")       proc.setLiveSlide (false);
    else if (liveButton == "hold")        proc.setLiveHold (false);
    else if (liveButton == "clear_rest")  proc.setLiveMute (false);
    else if (liveButton == "ratchet")     proc.setLiveRatchet (0);
    else if (liveButton == "gate_length") proc.setLiveGate (-1);
    liveButton = {};
}

//==============================================================================
int EditController::getDisplayValue() const
{
    // Active live ratchet/gate -> show the live working value.
    if (liveButton == "ratchet")     return liveRatchetVal;
    if (liveButton == "gate_length") return liveGateVal;

    // ARP: gate edit shows the arp gate; otherwise the arp mode (1..8).
    if (arpActive && mode == Mode::gateLengthEdit) return arpGateVal;

    switch (mode)
    {
        case Mode::octaveEdit:     return octaveShift;
        case Mode::patternSelect:
        case Mode::patternSave:    return selectedSlot;
        case Mode::gateLengthEdit: return editValue;
        case Mode::ratchetEdit:    return editValue;
        case Mode::stepEdit:       return editCursorStep + 1;   // step number
        case Mode::tempoEdit:
            return (int) std::lround (proc.apvts.getRawParameterValue ("TEMPO")->load());
        case Mode::keyboard:
        default:
            if (arpActive) return arpMode;                      // arp mode 1..8 (only outside tempoEdit)
            return (int) std::lround (proc.apvts.getRawParameterValue ("TEMPO")->load());
    }
}

bool EditController::buttonLit (const juce::String& id) const
{
    if (! liveButton.isEmpty() && id == liveButton) return true;   // active live override lights up
    if (id == "play_stop")   return proc.isSequencerPlaying();
    if (id == "rec_edit")    return recordActive || mode == Mode::stepEdit;
    if (id == "oct")         return mode == Mode::octaveEdit;
    if (id == "tempo_tap")   return mode == Mode::tempoEdit;
    if (id == "pattern")     return mode == Mode::patternSelect || mode == Mode::patternSave;  // co-blink on SAVE
    if (id == "save")        return mode == Mode::patternSave;
    if (id == "gate_length") return mode == Mode::gateLengthEdit;
    if (id == "ratchet")     return mode == Mode::ratchetEdit;
    if (id == "arp")         return arpActive;
    if (id == "hold")        return holdActive;
    if (id == "accent")      return accentLive || (editingStep() && cursorStep().accent);
    if (id == "slide")       return editingStep() && cursorStep().slide;
    return false;
}

bool EditController::buttonFlashing (const juce::String& id) const
{
    // Mode-edit buttons flash while their temporary mode is active (e.g. OCT
    // flashes in octave edit). REC/PLAY/ARP/HOLD stay steady.
    if (! buttonLit (id)) return false;
    return id == "oct" || id == "tempo_tap" || id == "pattern"
        || id == "save" || id == "gate_length" || id == "ratchet";
}

int EditController::uiSignature() const
{
    int s = (int) mode;
    s = s * 131 + editCursorStep;
    s = s * 131 + getDisplayValue();
    s = s * 7   + (recordActive ? 1 : 0);
    s = s * 7   + (arpActive    ? 1 : 0);
    s = s * 7   + (holdActive   ? 1 : 0);
    s = s * 7   + (accentLive   ? 1 : 0);
    s = s * 7   + (armStepSelect ? 1 : 0);
    s = s * 7   + (isCursorActive() ? 1 : 0);
    s = s * 131 + (liveButton.isEmpty() ? 0 : liveButton.hashCode());
    s = s * 7   + (liveTransposeOn ? 1 : 0);
    return s;
}
