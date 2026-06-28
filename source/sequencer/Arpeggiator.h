#pragma once

#include <array>

// Arpeggiator — juce-free and independently unit-testable. Replaces the sequencer
// as note source once active: held keys form a chord buffer, which is stepped through
// according to the mode. One note per (1/16) step, like a repeated one-step pattern.
//
// Allocation-free: all storage is fixed-capacity (std::array) and the expanded step
// sequence is rebuilt only when the chord/mode changes, so noteOn/noteOff/nextNote
// never touch the heap — safe to drive from the audio thread.
class Arpeggiator
{
public:
    enum class Mode
    {
        up1 = 1,        // 1: ascending
        down1,          // 2: descending
        downUp,         // 3: down, then up (no doubling of the end notes)
        random,         // 4: random from the buffer
        up2plus1,       // 5: ascending, then the same notes +12
        down2plus1,     // 6: descending, then the same notes +12 descending
        up3minus1,      // 7: ascending, then the same notes -12
        down3minus1     // 8: descending, then the same notes -12 descending
    };

    // --- Chord buffer (keys pressed/released) ---------------------------------
    void noteOn (int midi);
    void noteOff (int midi);
    void clear();

    void setMode (int modeNum);        // 1..8
    void setHold (bool b);             // HOLD: arp keeps running after release

    bool hasNotes() const { return bufferCount > 0; }
    int  size() const     { return bufferCount; }

    // Advances one arp step and returns the MIDI note (-1 if empty).
    int  nextNote();

    // (tests only) current mode / hold status
    Mode getMode() const  { return mode; }
    bool getHold() const  { return holdActive; }

private:
    static constexpr int kMaxChord = 128;   // distinct MIDI notes that can be held at once
    static constexpr int kMaxSeq   = 256;   // up to 2x the chord for the +/- octave modes

    static bool contains     (const std::array<int, kMaxChord>& a, int count, int x);
    static void insertSorted (std::array<int, kMaxChord>& a, int& count, int x);
    static void eraseValue   (std::array<int, kMaxChord>& a, int& count, int x);
    void rebuildSequence();            // rebuild `seq` from `buffer` + `mode`

    std::array<int, kMaxChord> held   {};   int heldCount   = 0;   // physically pressed keys (sorted, unique)
    std::array<int, kMaxChord> buffer {};   int bufferCount = 0;   // step buffer (persists during HOLD)
    std::array<int, kMaxSeq>   seq    {};   int seqCount    = 0;   // expanded sequence (rebuilt on change)
    bool   holdActive = false;
    Mode   mode       = Mode::up1;
    int    stepIndex  = 0;
    unsigned rng      = 0x9e3779b9u;   // simple LCG for RANDOM
};
