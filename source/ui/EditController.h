#pragma once

#include "PluginProcessor.h"

// Modal state machine for the 5×3 button grid + step editing.
// Handles the step-write and step-edit flow.
//
// Lives entirely on the message thread (the editor). Does NOT mutate the Pattern
// directly but via proc.postPatternCommand() (lock-free to the audio thread).
// Transport (PLAY) and TEMPO stay on the APVTS so host automation works.
//
// The hardware gesture "hold REC/EDIT + tap step" is translated for the mouse:
// long-pressing REC/EDIT ARMS step-select; the next step click selects it for stepEdit.
class EditController
{
public:
    enum class Mode
    {
        keyboard,        // default — keys play the synth
        tempoEdit,       // ▲/▼ adjust tempo; display = BPM
        octaveEdit,      // ▲/▼ adjust octave shift (keyboard)
        patternSelect,   // ▲/▼ choose slot to load
        patternSave,     // ▲/▼ choose slot; SAVE confirms
        stepEdit,        // edit one specific step
        gateLengthEdit,  // ▲/▼ gate length of the cursor step
        ratchetEdit      // ▲/▼ ratchet of the cursor step
    };

    explicit EditController (BassSeqProcessor& p) : proc (p) {}

    // --- Result of a key press (natural/sharp) -----------------------
    struct KeyResult { bool sound = false; int midi = -1; bool accent = false; };

    // Key pressed. stepIndex = 0..15 for naturals, -1 for sharps.
    KeyResult keyDown (int stepIndex, int baseMidi, double nowMs);
    void      keyUp   (int stepIndex, double nowMs);

    // Grid buttons (the 5×3). down/up separated for long-press detection.
    void gridButtonDown (const juce::String& id, double nowMs);
    void gridButtonUp   (double nowMs);

    // 30 Hz tick: handle long-press, mode timeout and ▲/▼ repeat.
    void tick (double nowMs);

    // --- Queries for the render ------------------------------------------------
    Mode getMode() const             { return mode; }
    int  getEditCursorStep() const   { return editCursorStep; }
    int  getOctaveShift() const      { return octaveShift; }
    bool isCursorActive() const;            // show cursor highlight on the pads?
    int  getDisplayValue() const;           // what the 7-segment shows
    bool displayIsSigned() const     { return mode == Mode::octaveEdit; }
    bool buttonLit (const juce::String& id) const;   // LED state per grid button
    bool buttonFlashing (const juce::String& id) const;  // flashing mode LED (vs steady glow)
    bool isHoldActive() const  { return holdActive; }    // keyboard sustain (HOLD)
    void nudgeOctave (int dir);             // direct octave shift (computer key Z/X)
    int  uiSignature() const;               // change detection for repaint

private:
    // --- Actions ----------------------------------------------------------------
    void shortAction (const juce::String& id, double nowMs);
    void applyArrow  (int dir, double nowMs);
    void maybeFireLong (double nowMs);
    void registerTap (double nowMs);

    void advanceCursor (int dir);
    void refreshEditValue();
    void touch (double nowMs) { lastInteraction = nowMs; }

    bool editingStep() const;               // is the cursor on an editable step?
    Step cursorStep() const { return proc.getStepForDisplay (editCursorStep); }
    int  inheritedGate() const;             // gate from the previous step (rest → fallback)

    void post (PatternCommand::Type t, int step, int note, int value);

    void togglePlay();
    void nudgeTempo (int dir);

    // --- Live overrides (button held during playback) -----------------
    bool isLiveButton (const juce::String& id) const;
    void startLive (const juce::String& id, double nowMs);   // activate override
    void endLive();                                          // end override

    BassSeqProcessor& proc;

    Mode mode          = Mode::keyboard;
    bool recordActive  = false;
    bool arpActive     = false;
    bool holdActive    = false;
    bool accentLive    = false;             // live keyboard accent (ACCENT in keyboard mode)

    int  editCursorStep = 0;                // 0..15
    int  octaveShift    = 0;                // -3..+3
    int  selectedSlot   = 1;                // 1..128 (load/save target)
    int  arpMode        = 1;                // 1..8 arp mode
    int  arpGateVal     = 8;                // 1..8 arp gate length
    bool arpAccentOn    = false;            // global accent for all arp notes
    int  editValue      = 4;                // working value gate/ratchet edit + display
    int  lastWrittenGate = 4;               // gate inheritance on step-write

    // --- gesture/timing state --------------------------------------------------
    double lastInteraction = 0.0;

    juce::String heldButton;                // grid button currently pressed ("" = none)
    double heldButtonTime    = 0.0;
    bool   heldButtonLong    = false;
    bool   armStepSelect     = false;       // REC/EDIT long → next step click = stepEdit
    double armTime           = 0.0;         // when armed (for the arm timeout)

    int    heldArrow         = 0;           // +1 / -1 / 0
    double nextArrowRepeat   = 0.0;

    int    heldStep          = -2;          // -2 none, -1 sharp, 0..15 natural
    double heldStepTime      = 0.0;
    bool   heldStepLong      = false;

    // live-override state
    juce::String liveButton;                // grid button with active live override ("" = none)
    int    liveRatchetVal    = 3;           // working value live ratchet (▲/▼, 1..4)
    int    liveGateVal       = 2;           // working value live gate (▲/▼, 0..8)
    bool   liveTransposeOn   = false;       // key transposition active during playback

    std::vector<double> tapTimes;           // tap tempo

    static constexpr double kModeTimeoutMs = 5000.0;
    static constexpr double kLongPressMs   = 2000.0;
    static constexpr double kArmPressMs    = 500.0;
    static constexpr double kArmTimeoutMs  = 4000.0;   // armed step-select expires by itself
    static constexpr double kArrowDelayMs  = 300.0;
    static constexpr double kArrowRepeatMs = 50.0;     // ~20 Hz
    static constexpr double kLiveHoldMs    = 250.0;    // button > this pressed during playback → live override
};
