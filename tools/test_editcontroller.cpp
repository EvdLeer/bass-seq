// Headless logic test for the editing state machine. Drives EditController
// directly (clicking is impossible without a display) and drains the edit-command FIFO
// via a silent processBlock, exactly as the real audio thread does.
#include "PluginProcessor.h"
#include "ui/EditController.h"
#include "sequencer/PatternBank.h"
#include "sequencer/PatternStorage.h"
#include "sequencer/StepSequencer.h"
#include "sequencer/Arpeggiator.h"
#include "GlobalSettings.h"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  PASS  %s\n", msg); } \
    else      { std::printf("  FAIL  %s\n", msg); ++g_fail; } } while (0)

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;   // MessageManager for APVTS

    BassSeqProcessor proc;
    proc.prepareToPlay (44100.0, 512);

    EditController edit (proc);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    auto drain = [&] { buf.clear(); midi.clear(); proc.processBlock (buf, midi); };
    auto processN = [&] (int samples) {
        int done = 0;
        while (done < samples) {
            const int n = juce::jmin (512, samples - done);
            juce::AudioBuffer<float> b (2, n); b.clear();
            juce::MidiBuffer m; proc.processBlock (b, m);
            done += n;
        }
    };
    auto stepsEqual = [] (const Pattern& a, const Pattern& b) {
        for (size_t i = 0; i < 16; ++i) if (a.steps[i] != b.steps[i]) return false;
        return true;
    };

    double t = 1000.0;
    auto press = [&] (const char* id) { edit.gridButtonDown (id, t); edit.gridButtonUp (t); t += 10.0; };

    std::printf ("[1] REC step-write + gate inheritance\n");
    press ("rec_edit");                                  // -> record on, cursor 0
    CHECK (edit.buttonLit ("rec_edit"), "REC LED on after REC/EDIT");
    edit.keyDown (0, 36, t); edit.keyUp (0, t); t += 10;  // write note 36 -> step 0, cursor -> 1
    edit.keyDown (1, 38, t); edit.keyUp (1, t); t += 10;  // write note 38 -> step 1, cursor -> 2
    drain();
    CHECK (proc.getStepForDisplay (0).note == 36, "step 0 note = 36 written");
    CHECK (proc.getStepForDisplay (0).gateLength == 4, "step 0 gate = 4 (inherited)");
    CHECK (proc.getStepForDisplay (1).note == 38, "step 1 note = 38 written");
    CHECK (edit.getEditCursorStep() == 2, "cursor advanced to 2");

    std::printf ("[2] CLEAR/REST writes rest + advances\n");
    press ("clear_rest");                                // rest on step 2, cursor -> 3
    drain();
    CHECK (proc.getStepForDisplay (2).gateLength == 0, "step 2 = rest (gate 0)");
    CHECK (edit.getEditCursorStep() == 3, "cursor advanced to 3");

    std::printf ("[3] Per-step ACCENT/SLIDE toggle in edit context\n");
    // cursor back to 0 via REC re-toggle.
    press ("rec_edit"); press ("rec_edit");              // off and on again -> cursor 0
    CHECK (edit.getEditCursorStep() == 0, "REC again -> cursor 0");
    press ("accent"); press ("slide");
    drain();
    CHECK (proc.getStepForDisplay (0).accent, "step 0 accent set");
    CHECK (proc.getStepForDisplay (0).slide,  "step 0 slide set");
    press ("accent");                                    // off again
    drain();
    CHECK (! proc.getStepForDisplay (0).accent, "step 0 accent off again");

    std::printf ("[4] Gate-length edit with the arrows\n");
    press ("gate_length");
    CHECK (edit.getMode() == EditController::Mode::gateLengthEdit, "mode = gateLengthEdit");
    const int g0 = proc.getStepForDisplay (0).gateLength;
    edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10;   // gate +1
    drain();
    CHECK (proc.getStepForDisplay (0).gateLength == g0 + 1, "gate +1 via up-arrow");
    CHECK (edit.getDisplayValue() == g0 + 1, "display shows new gate");
    press ("gate_length");                               // leave edit mode

    std::printf ("[5] Ratchet edit (clamp 1..4)\n");
    press ("ratchet");
    for (int i = 0; i < 6; ++i) { edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10; }
    drain();
    CHECK (proc.getStepForDisplay (0).ratchet == 4, "ratchet clamps at 4");
    CHECK (edit.getDisplayValue() == 4, "display shows ratchet 4");
    press ("ratchet");

    std::printf ("[6] Step edit via armed REC/EDIT long-press\n");
    press ("rec_edit");                                  // record off (was still on)
    edit.gridButtonDown ("rec_edit", t);                 // hold down...
    edit.tick (t + 600.0);                               // > 500 ms -> step-select armed
    edit.gridButtonUp (t + 650.0); t += 700.0;           // release: no short action
    auto sel = edit.keyDown (5, 41, t); edit.keyUp (5, t); t += 10;  // 1st click: SELECT step 5
    CHECK (edit.getMode() == EditController::Mode::stepEdit, "mode = stepEdit");
    CHECK (edit.getEditCursorStep() == 5, "cursor on selected step 5");
    CHECK (! sel.sound, "selecting makes no sound / does not write");
    edit.keyDown (-1, 60, t); edit.keyUp (-1, t); t += 10;          // 2nd press: set note (no advance)
    drain();
    CHECK (proc.getStepForDisplay (5).note == 60, "note 60 on step 5 (stepEdit, no advance)");
    CHECK (edit.getEditCursorStep() == 5, "cursor stays on 5 (stepEdit does not advance)");
    press ("rec_edit");                                  // leave stepEdit

    std::printf ("[7] Octave edit + transposed key note\n");
    press ("oct");
    CHECK (edit.getMode() == EditController::Mode::octaveEdit, "mode = octaveEdit");
    edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10;   // octave +1
    CHECK (edit.getOctaveShift() == 1, "octave-shift = +1");
    CHECK (edit.getDisplayValue() == 1, "display shows octave 1");
    CHECK (edit.displayIsSigned(), "octave display is signed");
    auto r = edit.keyDown (8, 48, t); edit.keyUp (8, t); t += 10;    // C3 = 48, +12 -> 60
    CHECK (r.sound && r.midi == 60, "key sounds 12 semitones higher (48->60)");
    press ("oct");

    std::printf ("[8] Tempo edit sets the TEMPO parameter\n");
    const int tempo0 = (int) proc.apvts.getRawParameterValue ("TEMPO")->load();
    press ("tempo_tap");
    CHECK (edit.getMode() == EditController::Mode::tempoEdit, "mode = tempoEdit");
    edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10;
    CHECK ((int) proc.apvts.getRawParameterValue ("TEMPO")->load() == tempo0 + 1, "TEMPO +1 via arrow");

    std::printf ("[9] Mode timeout falls back to keyboard\n");
    edit.tick (t + 6000.0);                              // > 5 s inactive
    CHECK (edit.getMode() == EditController::Mode::keyboard, "tempoEdit timed out -> keyboard");

    std::printf ("[10] PLAY/STOP toggle drives the PLAY parameter\n");
    const bool play0 = proc.apvts.getRawParameterValue ("PLAY")->load() > 0.5f;
    edit.gridButtonDown ("play_stop", t); edit.gridButtonUp (t); t += 10;
    drain();
    CHECK ((proc.apvts.getRawParameterValue ("PLAY")->load() > 0.5f) != play0, "PLAY parameter toggled");
    proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f); drain();   // stop again: edit tests assume stopped

    std::printf ("[11] ARP/HOLD flags toggle (LED)\n");
    press ("arp"); press ("hold");
    CHECK (edit.buttonLit ("arp"),  "ARP LED on");
    CHECK (edit.buttonLit ("hold"), "HOLD LED on");
    press ("arp"); press ("hold");                       // off again for the next tests
    CHECK (! edit.buttonLit ("arp") && ! edit.buttonLit ("hold"), "ARP/HOLD off again");

    std::printf ("[12] tempoEdit shows BPM even with ARP on (SM-002)\n");
    press ("arp");                                       // arp on
    press ("tempo_tap");                                 // tempoEdit
    const int tBpm = (int) proc.apvts.getRawParameterValue ("TEMPO")->load();
    CHECK (edit.getDisplayValue() == tBpm, "tempoEdit shows BPM, not arp mode");
    edit.tick (t + 6000.0); t += 6100.0;                 // timeout -> keyboard
    press ("arp");                                       // arp off again

    std::printf ("[13] Mode LED flashes (OCT-02)\n");
    press ("oct");
    CHECK (edit.buttonFlashing ("oct"),        "OCT LED flashes in octave edit");
    CHECK (! edit.buttonFlashing ("play_stop"),"PLAY LED does not flash");
    press ("oct");                                       // octaveEdit off again

    std::printf ("[14] Expired step-select arm does not swallow a pad click (SE-01)\n");
    edit.gridButtonDown ("rec_edit", t); edit.tick (t + 600.0); edit.gridButtonUp (t + 650.0); t += 700.0;
    edit.tick (t + 5000.0); t += 5100.0;                 // arm > 4 s -> expires
    { auto k = edit.keyDown (7, 45, t); edit.keyUp (7, t); t += 10.0;
      CHECK (edit.getMode() != EditController::Mode::stepEdit, "expired arm -> no stepEdit");
      CHECK (k.sound, "pad plays normally after expired arm"); }

    std::printf ("[15] Another button cancels a pending arm (SE-01)\n");
    edit.gridButtonDown ("rec_edit", t); edit.tick (t + 600.0); edit.gridButtonUp (t + 650.0); t += 700.0;
    press ("gate_length");                               // other grid button -> arm cancelled
    edit.keyDown (7, 45, t); edit.keyUp (7, t); t += 10.0;
    CHECK (edit.getMode() != EditController::Mode::stepEdit, "cancelled arm -> no stepEdit");
    press ("gate_length");                               // leave edit mode

    std::printf ("[16] CLEAR/REST toggles REST in stepEdit (SE-08)\n");
    edit.gridButtonDown ("rec_edit", t); edit.tick (t + 600.0); edit.gridButtonUp (t + 650.0); t += 700.0;
    edit.keyDown (0, 35, t); edit.keyUp (0, t); t += 10.0;   // select step 0 -> stepEdit
    CHECK (edit.getMode() == EditController::Mode::stepEdit, "stepEdit on step 0");
    CHECK (proc.getStepForDisplay (0).gateLength > 0, "step 0 has a gate before toggle");
    press ("clear_rest"); drain();
    CHECK (proc.getStepForDisplay (0).gateLength == 0, "CLEAR/REST in stepEdit -> rest");
    press ("clear_rest"); drain();
    CHECK (proc.getStepForDisplay (0).gateLength > 0, "CLEAR/REST again -> restored");
    press ("rec_edit");                                  // leave stepEdit

    std::printf ("[17] Written step inherits gate from the previous step (SW-14)\n");
    { PatternCommand c; c.type = PatternCommand::Type::setGate; c.step = 4; c.value = 2; proc.postPatternCommand (c); }
    drain();
    press ("rec_edit");                                  // record on, cursor 0
    for (int i = 0; i < 5; ++i) { edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10.0; }  // cursor -> 5
    edit.keyDown (5, 41, t); edit.keyUp (5, t); t += 10.0;   // write on step 5
    drain();
    CHECK (proc.getStepForDisplay (5).gateLength == 2, "step 5 inherits gate 2 from step 4");
    press ("rec_edit");                                  // record off

    // ===== Pattern memory =========================================
    std::printf ("[18] Pattern (de)serialisation round-trip\n");
    {
        Pattern p; p.id = 7; p.tempo = 137.0; p.length = 12; p.transpose = -3;
        p.steps[0] = Step { 40, 5, true,  false, 2 };
        p.steps[3] = Step { 55, 8, false, true,  1 };
        p.steps[9] = Step { 60, 0, false, false, 4 };
        const Pattern q = patternFromValueTree (patternToValueTree (p));
        CHECK (q.id == 7 && q.length == 12 && q.transpose == -3, "id/length/transpose round-trip");
        CHECK (std::abs (q.tempo - 137.0) < 1e-6, "tempo round-trip");
        CHECK (stepsEqual (p, q), "all 16 steps round-trip identical");
    }

    // Point the bank at a TEMP file so the real app data is not touched.
    const auto tempBank = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bass_seq_test_banks.xml");
    tempBank.deleteFile();
    proc.setBankFile (tempBank); drain();
    proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f); drain();   // stop sequencer -> loads apply immediately

    std::printf ("[19] Bank writes to file and reloads identically\n");
    proc.saveWorkingToSlot (7);
    {
        const Pattern working = proc.getWorkingPatternCopy();
        CHECK (tempBank.existsAsFile(), "bank file written");
        PatternBank fresh; fresh.setFile (tempBank); fresh.load();
        CHECK (fresh.has (7), "slot 7 present after reload");
        CHECK (fresh.lastSlot() == 7, "lastSlot = 7 preserved");
        CHECK (stepsEqual (fresh.get (7), working), "loaded slot 7 == saved working pattern");
    }

    std::printf ("[20] SAVE/PATTERN modes: save -> clear -> reload\n");
    {
        const Pattern saved = proc.getWorkingPatternCopy();
        press ("save");                                  // patternSave, selectedSlot = currentSlot (7)
        edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10;
        edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10;   // -> slot 9
        press ("save");                                  // confirm -> saveWorkingToSlot(9)
        CHECK (proc.getCurrentSlot() == 9, "saved in slot 9");
        CHECK (proc.isSlotUsed (9), "slot 9 marked as used");

        { PatternCommand c; c.type = PatternCommand::Type::initPattern; proc.postPatternCommand (c); } drain();
        CHECK (proc.getStepForDisplay (0).gateLength == 0 && proc.getStepForDisplay (0).note < 0,
               "pattern cleared after initPattern");

        press ("pattern");                               // patternSelect, selectedSlot = 9
        edit.gridButtonDown ("down", t); edit.gridButtonUp (t); t += 10;  // -> 8 (empty) loads
        edit.gridButtonDown ("up",   t); edit.gridButtonUp (t); t += 10;  // -> 9 loads our pattern
        drain();
        CHECK (stepsEqual (proc.getWorkingPatternCopy(), saved), "slot 9 reloaded == saved pattern");
    }

    std::printf ("[21] Deferred load is only applied at the pattern boundary\n");
    {
        Pattern A; for (auto& s : A.steps) s = Step { 40, 4, false, false, 1 }; A.tempo = 120.0; A.id = 10;
        Pattern B; for (auto& s : B.steps) s = Step { 50, 4, false, false, 1 }; B.tempo = 120.0; B.id = 11;
        // Put slots 10/11 in the bank file via a separate bank, then reload the proc bank.
        { PatternBank w; w.setFile (tempBank); w.load(); w.put (10, A); w.put (11, B); w.save(); }
        proc.setBankFile (tempBank); drain();

        proc.loadSlot (10); drain();                     // stopped -> immediate
        CHECK (stepsEqual (proc.getWorkingPatternCopy(), A), "slot 10 (A) loaded immediately while stopped");

        proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (1.0f); drain();   // sequencer starts
        proc.loadSlot (11);                              // playing -> deferred until the boundary
        processN (2000);
        CHECK (stepsEqual (proc.getWorkingPatternCopy(), A), "B NOT yet applied before the pattern boundary");
        processN (90000);                                // > 1 pattern (88200 spl @120) -> boundary passed
        CHECK (stepsEqual (proc.getWorkingPatternCopy(), B), "B applied AFTER the pattern boundary");
        proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f); drain();   // stop
    }

    std::printf ("[22] Plugin state saves and restores working pattern + slot\n");
    {
        const Pattern before = proc.getWorkingPatternCopy();
        const int     slot   = proc.getCurrentSlot();
        juce::MemoryBlock blob; proc.getStateInformation (blob);

        BassSeqProcessor proc2; proc2.prepareToPlay (44100.0, 512);
        proc2.setStateInformation (blob.getData(), (int) blob.getSize());
        { juce::AudioBuffer<float> b (2, 512); b.clear(); juce::MidiBuffer m; proc2.processBlock (b, m); }
        CHECK (proc2.getCurrentSlot() == slot, "slot restored from plugin state");
        CHECK (stepsEqual (proc2.getWorkingPatternCopy(), before), "working pattern restored from plugin state");
    }

    std::printf ("[23] Last-LOADED slot is recorded globally (ST-18)\n");
    proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f); drain();
    proc.loadSlot (10); drain();
    {
        PatternBank fr; fr.setFile (tempBank); fr.load();
        CHECK (fr.lastSlot() == 10, "bank file remembers last-loaded slot 10");
    }

    std::printf ("[24] Newer bank version -> safe fallback to empty (ST-30/31)\n");
    {
        const auto vf = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bass_seq_test_future.xml");
        juce::ValueTree tv ("BASSSEQBANKS");
        tv.setProperty ("version", 99, nullptr);
        tv.setProperty ("lastPattern", 5, nullptr);
        auto pt = patternToValueTree (Pattern{}); pt.setProperty ("slot", 5, nullptr);
        tv.appendChild (pt, nullptr);
        if (auto x = tv.createXml()) x->writeTo (vf);
        PatternBank nb; nb.setFile (vf); nb.load();
        CHECK (nb.countUsed() == 0, "newer version is not loaded (empty bank)");
        vf.deleteFile();
    }

    std::printf ("[25] PATTERN LED flashes along in SAVE mode (PM-04)\n");
    press ("save");
    CHECK (edit.buttonLit ("save"),    "SAVE LED on in save mode");
    CHECK (edit.buttonLit ("pattern"), "PATTERN LED also on in save mode");
    CHECK (edit.buttonFlashing ("pattern"), "PATTERN LED flashes in save mode");
    edit.tick (t + 6000.0); t += 6100.0;                 // let save mode expire

    std::printf ("[26] Up/Down only loads on release in pattern-select (PM-12)\n");
    proc.loadSlot (10); drain();                         // working pattern = slot 10 (A: notes 40)
    press ("pattern");                                   // patternSelect, selectedSlot = 10
    edit.gridButtonDown ("up", t);                       // -> choose 11, NOT loading yet
    drain();
    CHECK (proc.getStepForDisplay (0).note == 40, "scrolling does not load yet (display-only)");
    edit.gridButtonUp (t); t += 10.0;                    // release -> load slot 11 (B: notes 50)
    drain();
    CHECK (proc.getStepForDisplay (0).note == 50, "Up/Down release loads the chosen slot");

    tempBank.deleteFile();

    // ===== Live performance overrides ===============================
    // (a) Direct StepSequencer test: overrides sit on top of the step data.
    auto runSeq = [] (const StepSequencer::LiveOverrides& lo, const Pattern& pat, int samples,
                      std::vector<int>& notes, std::vector<bool>& accents, int& endStep)
    {
        StepSequencer seq; seq.prepare (44100.0); seq.setPattern (&pat); seq.setLive (lo);
        seq.onNoteOn  = [&] (int n, bool a) { notes.push_back (n); accents.push_back (a); };
        seq.onNoteOff = [] {};
        seq.start();
        for (int i = 0; i < samples; ++i) seq.processSample();
        endStep = seq.getCurrentStep();
    };
    Pattern seqPat;
    for (size_t i = 0; i < 16; ++i) seqPat.steps[i] = Step { (int) (40 + i), 4, false, false, 1 };

    std::printf ("[27] Sequencer: live transposition shifts the notes\n");
    { std::vector<int> n; std::vector<bool> a; int es;
      StepSequencer::LiveOverrides lo; lo.transpose = 5;
      runSeq (lo, seqPat, 1, n, a, es);
      CHECK (! n.empty() && n[0] == 45, "step 0 (note 40) sounds transposed as 45"); }

    std::printf ("[28] Sequencer: live mute suppresses note-ons, timing keeps running\n");
    { std::vector<int> n; std::vector<bool> a; int es;
      StepSequencer::LiveOverrides lo; lo.mute = true;
      runSeq (lo, seqPat, 12000, n, a, es);     // > 2 steps
      CHECK (n.empty(), "no note-on at all during mute");
      CHECK (es != 0, "scheduler did advance (timing in sync)"); }

    std::printf ("[29] Sequencer: force-accent sets accent on every step\n");
    { std::vector<int> n; std::vector<bool> a; int es;
      StepSequencer::LiveOverrides lo; lo.accent = true;
      runSeq (lo, seqPat, 1, n, a, es);
      CHECK (! a.empty() && a[0], "step 0 sounds accented"); }

    std::printf ("[30] Sequencer: live HOLD repeats the current step (no advance)\n");
    { std::vector<int> n; std::vector<bool> a; int es;
      StepSequencer::LiveOverrides lo; lo.hold = true;
      runSeq (lo, seqPat, 12000, n, a, es);     // > 2 step durations
      CHECK (es == 0, "cursor stays on step 0");
      CHECK (n.size() >= 2 && n[0] == 40 && n[1] == 40, "same step (note 40) repeats"); }

    std::printf ("[31] Sequencer: live-ratchet override re-triggers N times\n");
    { std::vector<int> n; std::vector<bool> a; int es;
      StepSequencer::LiveOverrides lo; lo.ratchet = 4;
      runSeq (lo, seqPat, 5000, n, a, es);      // within 1 step (sps~5512 @120)
      CHECK (n.size() == 4, "4 sub-triggers in step 0 from live-ratchet 4"); }

    // (b) EditController gesture -> processor atomics (hold button during playback).
    proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (1.0f); drain();   // sequencer playing

    std::printf ("[32] Hold ACCENT during playback -> live accent\n");
    edit.gridButtonDown ("accent", t); edit.tick (t + 300.0);
    CHECK (proc.getLiveAccent(), "ACCENT held > 250 ms -> live accent on");
    CHECK (edit.buttonLit ("accent"), "ACCENT LED on during live override");
    edit.gridButtonUp (t + 350.0); t += 400.0;
    CHECK (! proc.getLiveAccent(), "release -> live accent off");

    std::printf ("[33] Short tap != live (does the short action)\n");
    {
        const bool before = edit.buttonLit ("accent");      // = accentLive
        edit.gridButtonDown ("accent", t); edit.gridButtonUp (t + 50.0); t += 60.0;  // < 250 ms, no tick
        CHECK (! proc.getLiveAccent(), "short tap -> no live accent");
        CHECK (edit.buttonLit ("accent") != before, "short tap does toggle the live-accent flag");
        edit.gridButtonDown ("accent", t); edit.gridButtonUp (t + 50.0); t += 60.0;  // reset
    }

    std::printf ("[34] Hold CLEAR/REST during playback -> live mute\n");
    edit.gridButtonDown ("clear_rest", t); edit.tick (t + 300.0);
    CHECK (proc.getLiveMute(), "CLEAR/REST held during playback -> mute");
    edit.gridButtonUp (t + 350.0); t += 400.0;
    CHECK (! proc.getLiveMute(), "release -> mute off");

    std::printf ("[35] Hold RATCHET -> live ratchet (default 3)\n");
    edit.gridButtonDown ("ratchet", t); edit.tick (t + 300.0);
    CHECK (proc.getLiveRatchet() == 3, "live-ratchet with working value 3");
    CHECK (edit.getDisplayValue() == 3, "display shows the live-ratchet value");
    edit.gridButtonUp (t + 350.0); t += 400.0;
    CHECK (proc.getLiveRatchet() == 0, "release -> live ratchet off");

    std::printf ("[36] Key during playback -> live transposition (literally key - 48)\n");
    {
        edit.keyDown (15, 60, t); t += 10.0;     // raw key MIDI 60, octave does NOT count
        CHECK (proc.getLiveTranspose() == 60 - 48, "key sets live transposition (60 - 48 = 12)");
        edit.keyUp (15, t); t += 10.0;
        CHECK (proc.getLiveTranspose() == 0, "key release -> transposition back to 0");
    }

    std::printf ("[37] Override is lifted when playback stops while button is held (L01)\n");
    edit.gridButtonDown ("accent", t); edit.tick (t + 300.0);     // live accent active (still playing)
    CHECK (proc.getLiveAccent(), "live accent active before stop");
    proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f); drain();   // sequencer stops
    edit.tick (t + 350.0); t += 400.0;
    CHECK (! proc.getLiveAccent(), "stop while holding -> override lifted");
    edit.gridButtonUp (t); t += 10.0;                            // release button cleanly

    std::printf ("[38] Sequencer: live mute releases a hanging TIE note (L21)\n");
    {
        Pattern tie;
        tie.steps[0] = Step { 40, 8, false, false, 1 };          // TIE
        tie.steps[1] = Step { 42, 4, false, false, 1 };          // next = not a rest -> no note-off scheduled
        StepSequencer seq; seq.prepare (44100.0); seq.setPattern (&tie);
        int offs = 0; std::vector<int> ons;
        seq.onNoteOn  = [&] (int n, bool) { ons.push_back (n); };
        seq.onNoteOff = [&] { ++offs; };
        seq.start(); seq.processSample();                        // step 0: TIE note on, no note-off
        const int offsBefore = offs;
        StepSequencer::LiveOverrides lo; lo.mute = true;
        seq.setLive (lo);                                        // enable mute
        CHECK (offs == offsBefore + 1, "mute releases the hanging TIE note (note-off)");
    }

    // ===== Arpeggiator ==============================================
    std::printf ("[39] Arp: chord buffer + mode sequences\n");
    {
        Arpeggiator ap; ap.setMode (1);                  // UP_1
        ap.noteOn (43); ap.noteOn (36); ap.noteOn (40);  // out of order -> must be sorted
        CHECK (ap.size() == 3, "3 notes in the buffer");
        std::vector<int> up;
        for (int i = 0; i < 6; ++i) up.push_back (ap.nextNote());
        CHECK ((up == std::vector<int> { 36, 40, 43, 36, 40, 43 }), "UP_1 = ascending, repeats");

        Arpeggiator dn; dn.setMode (2);                  // DOWN_1
        dn.noteOn (36); dn.noteOn (40); dn.noteOn (43);
        std::vector<int> d; for (int i = 0; i < 4; ++i) d.push_back (dn.nextNote());
        CHECK ((d == std::vector<int> { 43, 40, 36, 43 }), "DOWN_1 = descending");

        Arpeggiator du; du.setMode (3);                  // DOWN_UP (no end doubling)
        du.noteOn (36); du.noteOn (40); du.noteOn (43);
        std::vector<int> u; for (int i = 0; i < 8; ++i) u.push_back (du.nextNote());
        CHECK ((u == std::vector<int> { 43, 40, 36, 40, 43, 40, 36, 40 }), "DOWN_UP = down, then up without doubling");

        Arpeggiator u2; u2.setMode (5);                  // UP_2_PLUS_1
        u2.noteOn (36); u2.noteOn (40);
        std::vector<int> o; for (int i = 0; i < 4; ++i) o.push_back (u2.nextNote());
        CHECK ((o == std::vector<int> { 36, 40, 48, 52 }), "UP_2+1 = ascending, then +12");
    }

    std::printf ("[39b] Arp: all 8 modes golden-value on [60,64,67]\n");
    {
        auto seqOf = [] (int modeNum, int len) {
            Arpeggiator a; a.setMode (modeNum);
            a.noteOn (67); a.noteOn (60); a.noteOn (64);  // out of order -> sorts
            std::vector<int> v; for (int i = 0; i < len; ++i) v.push_back (a.nextNote());
            return v;
        };
        CHECK ((seqOf (1, 3) == std::vector<int> { 60, 64, 67 }),                 "1 UP_1");
        CHECK ((seqOf (2, 3) == std::vector<int> { 67, 64, 60 }),                 "2 DOWN_1");
        CHECK ((seqOf (3, 4) == std::vector<int> { 67, 64, 60, 64 }),             "3 DOWN_UP");
        CHECK ((seqOf (5, 6) == std::vector<int> { 60, 64, 67, 72, 76, 79 }),     "5 UP_2+1OCT");
        CHECK ((seqOf (6, 6) == std::vector<int> { 67, 64, 60, 79, 76, 72 }),     "6 DOWN_2+1OCT");
        CHECK ((seqOf (7, 6) == std::vector<int> { 60, 64, 67, 48, 52, 55 }),     "7 UP_3-1OCT");
        CHECK ((seqOf (8, 6) == std::vector<int> { 67, 64, 60, 55, 52, 48 }),     "8 DOWN_3-1OCT");

        Arpeggiator rnd; rnd.setMode (4);                // RANDOM = only from the buffer
        rnd.noteOn (60); rnd.noteOn (64); rnd.noteOn (67);
        bool allInSet = true;
        for (int i = 0; i < 32; ++i) { const int n = rnd.nextNote();
            if (n != 60 && n != 64 && n != 67) allInSet = false; }
        CHECK (allInSet, "4 RANDOM draws exclusively from the chord buffer");
    }

    std::printf ("[40] Arp HOLD: stays after release; new chord replaces\n");
    {
        Arpeggiator ap; ap.setHold (true);
        ap.noteOn (36); ap.noteOn (40);
        ap.noteOff (36); ap.noteOff (40);                // all keys released
        CHECK (ap.hasNotes(), "HOLD: buffer stays after release");
        ap.noteOn (48);                                  // new chord (held was empty)
        CHECK (ap.size() == 1 && ap.nextNote() == 48, "new chord replaces the old buffer");

        Arpeggiator nh;                                  // without hold
        nh.noteOn (36); nh.noteOff (36);
        CHECK (! nh.hasNotes(), "without HOLD: release stops the arp");
    }

    std::printf ("[41] Sequencer: arp REPLACES the pattern as note source\n");
    {
        Arpeggiator ap; ap.setMode (1); ap.noteOn (60); ap.noteOn (64); ap.noteOn (67);
        StepSequencer seq; seq.prepare (44100.0); seq.setPattern (&seqPat); seq.setArp (&ap);
        std::vector<int> ons; seq.onNoteOn = [&] (int n, bool) { ons.push_back (n); }; seq.onNoteOff = [] {};
        seq.setArpParams (true, 8, false);               // arp active
        seq.start();
        for (int i = 0; i < 14000; ++i) seq.processSample();   // ~3 steps
        CHECK (ons.size() >= 3 && ons[0] == 60 && ons[1] == 64 && ons[2] == 67,
               "steps sound the arp notes (60,64,67), not the pattern notes");
        bool anyPattern = false; for (int n : ons) if (n >= 40 && n <= 55) anyPattern = true;
        CHECK (! anyPattern, "no pattern note at all during arp");
    }

    std::printf ("[42] Arp empty buffer -> silent steps\n");
    {
        Arpeggiator ap;                                  // no notes
        StepSequencer seq; seq.prepare (44100.0); seq.setPattern (&seqPat); seq.setArp (&ap);
        std::vector<int> ons; seq.onNoteOn = [&] (int n, bool) { ons.push_back (n); }; seq.onNoteOff = [] {};
        seq.setArpParams (true, 8, false);
        seq.start();
        for (int i = 0; i < 14000; ++i) seq.processSample();
        CHECK (ons.empty(), "no notes without a chord (arp replaces, pattern stays silent)");
    }

    std::printf ("[43] EditController: ARP buttons drive the processor parameters\n");
    edit.tick (t + 6000.0); t += 6100.0;                 // let any edit mode expire -> keyboard
    press ("arp");
    CHECK (proc.getArpActive(), "ARP on -> proc.arpActive");
    CHECK (edit.getDisplayValue() == proc.getArpMode(), "display shows arp mode");
    edit.gridButtonDown ("up", t); edit.gridButtonUp (t); t += 10.0;   // mode +1 (keyboard + arp)
    CHECK (proc.getArpMode() == 2, "Up -> arp mode 2");
    press ("accent");
    CHECK (proc.getArpAccent(), "ACCENT in arp -> global arp accent");
    press ("hold");
    CHECK (proc.getArpHold(), "HOLD -> arp hold");
    press ("gate_length");                               // gateLengthEdit (arp gate)
    edit.gridButtonDown ("down", t); edit.gridButtonUp (t); t += 10.0;  // arp gate 8 -> 7
    CHECK (proc.getArpGate() == 7, "Down in gate-edit -> arp gate 7");
    press ("gate_length");
    press ("hold");                                      // hold off again
    press ("arp");
    CHECK (! proc.getArpActive(), "ARP off again");

    // ===== MIDI I/O =================================================
    auto runBlock = [&] (juce::MidiBuffer& m) {
        juce::AudioBuffer<float> b (2, 512); b.clear(); proc.processBlock (b, m);
    };
    auto scanNoteOn = [] (juce::MidiBuffer& m, int note) {
        for (const auto md : m) { const auto mm = md.getMessage();
            if (mm.isNoteOn() && mm.getNoteNumber() == note) return (int) mm.getVelocity(); }
        return -1;
    };
    const auto tSet = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("bass_seq_test_settings.xml");
    tSet.deleteFile();
    proc.setSettingsFile (tSet);                         // saves to temp, not the real app data
    proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f); { juce::MidiBuffer e; runBlock (e); }

    std::printf ("[44] GlobalSettings round-trip to file\n");
    {
        GlobalSettings s; s.inputChannel = 1; s.inputTranspose = 2; s.outputChannel = 3;
        s.fwdSequencer = true; s.clockSource = 1; s.accentThreshold = 90; s.waitForPatternEnd = false;
        s.keyPriority = 2;
        proc.setSettings (s);
        CHECK (tSet.existsAsFile(), "settings.xml written");
        auto xml = juce::XmlDocument::parse (tSet);
        const GlobalSettings r = settingsFromValueTree (juce::ValueTree::fromXml (*xml));
        CHECK (r.inputChannel == 1 && r.inputTranspose == 2 && r.outputChannel == 3, "channel/transpose/out round-trip");
        CHECK (r.fwdSequencer && r.clockSource == 1 && r.accentThreshold == 90, "forwarding/clock/accent round-trip");
        CHECK (r.keyPriority == 2, "keyPriority round-trip");
        CHECK (! r.waitForPatternEnd, "waitForPatternEnd round-trip (settings, no longer runtime-only)");
    }

    std::printf ("[45] MIDI-in: channel filter + transpose + accent -> forward to out\n");
    {
        GlobalSettings s; s.inputChannel = 1; s.inputTranspose = 2; s.accentThreshold = 96;
        s.fwdKeyboard = true; s.outputChannel = 1; s.localControl = true; s.clockSource = 0;
        proc.setSettings (s);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);   // ch1, vel100>96 -> accent
        m.addEvent (juce::MidiMessage::noteOn (2, 70, (juce::uint8) 100), 1);   // ch2 -> filtered out
        runBlock (m);
        CHECK (scanNoteOn (m, 62) == 127, "ch1 note 60->62 (transpose) forwarded with accent velocity 127");
        CHECK (scanNoteOn (m, 72) == -1,  "ch2 filtered: no output");
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0); runBlock (off);
    }

    std::printf ("[46] MIDI-out: sequencer notes forwarded during playback\n");
    {
        GlobalSettings s; s.fwdSequencer = true; s.localControl = true; s.clockSource = 0;
        proc.setSettings (s);
        proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (1.0f);
        juce::MidiBuffer m; runBlock (m);                // start + step 0 fires on sample 0
        bool seqNote = false;
        for (const auto md : m) if (md.getMessage().isNoteOn()) seqNote = true;
        CHECK (seqNote, "sequencer note appears on MIDI-out");
        proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f);
        juce::MidiBuffer e; runBlock (e);
    }

    std::printf ("[47] MIDI-clock-out: Start + 24 PPQN + Stop (internal clock master)\n");
    {
        GlobalSettings s; s.fwdClock = true; s.clockSource = 0; proc.setSettings (s);
        proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (1.0f);
        juce::MidiBuffer m; runBlock (m);
        bool start = false, clk = false;
        for (const auto md : m) { const auto mm = md.getMessage();
            if (mm.isMidiStart()) start = true; if (mm.isMidiClock()) clk = true; }
        CHECK (start, "MIDI Start at playback begin");
        CHECK (clk,   "MIDI clock ticks on the output");
        proc.apvts.getParameter ("PLAY")->setValueNotifyingHost (0.0f);
        juce::MidiBuffer m2; runBlock (m2);
        bool stop = false; for (const auto md : m2) if (md.getMessage().isMidiStop()) stop = true;
        CHECK (stop, "MIDI Stop at playback end");
    }

    std::printf ("[48] MIDI-clock-in: Start/Stop transport + tempo follow (slave)\n");
    {
        GlobalSettings s; s.clockSource = 1; proc.setSettings (s);   // slave on MIDI
        juce::MidiBuffer mStart;
        mStart.addEvent (juce::MidiMessage::midiStart(), 0);
        mStart.addEvent (juce::MidiMessage::midiClock(), 0);
        runBlock (mStart);                               // transport on + 1st tick
        juce::MidiBuffer mTick;
        mTick.addEvent (juce::MidiMessage::midiClock(), 276);   // 2nd tick: interval ~ 512+276 = 788 spl ~ 140 BPM
        runBlock (mTick);                                // wantPlay (before loop) = true -> start
        CHECK (proc.isSequencerPlaying(), "MIDI Start -> sequencer plays (slave)");
        CHECK (std::abs (proc.getClockBpm() - 140.0) < 2.0, "tempo follows the incoming MIDI clock (~140)");
        juce::MidiBuffer mStop; mStop.addEvent (juce::MidiMessage::midiStop(), 0); runBlock (mStop);
        juce::MidiBuffer e; runBlock (e);
        CHECK (! proc.isSequencerPlaying(), "MIDI Stop -> sequencer stops");
    }

    std::printf ("[49] keyPriority wired (low/high) + settings version fallback\n");
    {
        // high -> highest pressed note wins (visible via the forwarding mirror)
        GlobalSettings s; s.keyPriority = 1; s.fwdKeyboard = true; s.localControl = true; s.inputChannel = 0;
        proc.setSettings (s);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 1);
        runBlock (m);
        CHECK (scanNoteOn (m, 64) != -1, "high-priority: highest note (64) wins and is forwarded");
        { juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
          off.addEvent (juce::MidiMessage::noteOff (1, 64), 1); runBlock (off); }

        // low -> lowest wins, the higher note does not trigger the voice
        GlobalSettings s2; s2.keyPriority = 0; s2.fwdKeyboard = true; s2.localControl = true; s2.inputChannel = 0;
        proc.setSettings (s2);
        juce::MidiBuffer m2;
        m2.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        m2.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 1);
        runBlock (m2);
        CHECK (scanNoteOn (m2, 64) == -1, "low-priority: lowest note wins, 64 does not trigger");
        { juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
          off.addEvent (juce::MidiMessage::noteOff (1, 64), 1); runBlock (off); }

        // version fallback: a newer settings file is ignored (current kept)
        GlobalSettings known; known.inputTranspose = 5; proc.setSettings (known);
        { auto t = settingsToValueTree (known); t.setProperty ("version", 99, nullptr);
          t.setProperty ("inputTranspose", 11, nullptr);
          if (auto x = t.createXml()) x->writeTo (tSet); }
        proc.setSettingsFile (tSet);
        CHECK (proc.getSettings().inputTranspose == 5, "newer settings version is ignored");
    }

    proc.setSettings (GlobalSettings{});                 // back to defaults
    tSet.deleteFile();

    // ===== UI polish ===============================================
    std::printf ("[50] nudgeOctave (computer key Z/X) clamp -3..+3\n");
    for (int i = 0; i < 10; ++i) edit.nudgeOctave (-1);
    CHECK (edit.getOctaveShift() == -3, "Z repeated -> clamp at -3");
    for (int i = 0; i < 10; ++i) edit.nudgeOctave (+1);
    CHECK (edit.getOctaveShift() == 3,  "X repeated -> clamp at +3");
    for (int i = 0; i < 3;  ++i) edit.nudgeOctave (-1);   // back to 0

    std::printf ("\n%s (%d failures)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail);
    return g_fail == 0 ? 0 : 1;
}
