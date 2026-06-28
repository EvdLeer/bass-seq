#include "Arpeggiator.h"

#include <cstddef>   // std::size_t

// Fixed-capacity, allocation-free chord/sequence handling (runs on the audio thread).

bool Arpeggiator::contains (const std::array<int, kMaxChord>& a, int count, int x)
{
    for (int i = 0; i < count; ++i)
        if (a[(std::size_t) i] == x) return true;
    return false;
}

void Arpeggiator::insertSorted (std::array<int, kMaxChord>& a, int& count, int x)
{
    if (count >= kMaxChord) return;        // full -> ignore (chord is bounded)
    if (contains (a, count, x)) return;    // no duplicates
    int pos = 0;
    while (pos < count && a[(std::size_t) pos] < x) ++pos;            // keep sorted ascending
    for (int i = count; i > pos; --i) a[(std::size_t) i] = a[(std::size_t) (i - 1)];
    a[(std::size_t) pos] = x;
    ++count;
}

void Arpeggiator::eraseValue (std::array<int, kMaxChord>& a, int& count, int x)
{
    int pos = 0;
    while (pos < count && a[(std::size_t) pos] != x) ++pos;
    if (pos == count) return;              // not found
    for (int i = pos; i < count - 1; ++i) a[(std::size_t) i] = a[(std::size_t) (i + 1)];
    --count;
}

void Arpeggiator::noteOn (int midi)
{
    const bool wasEmpty = (heldCount == 0);
    insertSorted (held, heldCount, midi);

    if (holdActive)
    {
        if (wasEmpty) { bufferCount = 0; stepIndex = 0; }   // new chord replaces the buffer
        insertSorted (buffer, bufferCount, midi);
    }
    else
    {
        if (wasEmpty) stepIndex = 0;                        // new attack -> start from the beginning
        buffer = held; bufferCount = heldCount;
    }
    rebuildSequence();
}

void Arpeggiator::noteOff (int midi)
{
    eraseValue (held, heldCount, midi);
    if (! holdActive)
    {
        buffer = held; bufferCount = heldCount;   // hold off -> buffer follows the pressed keys
    }
    // hold on -> buffer stays in place
    rebuildSequence();
}

void Arpeggiator::clear()
{
    heldCount   = 0;
    bufferCount = 0;
    seqCount    = 0;
    stepIndex   = 0;
}

void Arpeggiator::setMode (int modeNum)
{
    if (modeNum < 1) modeNum = 1;
    if (modeNum > 8) modeNum = 8;
    mode = (Mode) modeNum;      // switching is seamless (stepIndex stays in place)
    rebuildSequence();
}

void Arpeggiator::setHold (bool b)
{
    if (b == holdActive) return;   // important: pushArpParams sets this every block
    holdActive = b;
    if (! b)
    {
        buffer = held; bufferCount = heldCount;          // hold off -> back to live pressed keys
    }
    else if (bufferCount == 0)
    {
        buffer = held; bufferCount = heldCount;          // hold on without buffer -> snapshot of what is pressed
    }
    rebuildSequence();
}

void Arpeggiator::rebuildSequence()
{
    // `buffer` is already sorted ascending and unique.
    const int n = bufferCount;
    seqCount = 0;
    auto add = [this] (int v) { if (seqCount < kMaxSeq) seq[(std::size_t) seqCount++] = v; };

    switch (mode)
    {
        case Mode::up1:
        case Mode::random:                                       // selection happens in nextNote()
            for (int i = 0; i < n; ++i) add (buffer[(std::size_t) i]);
            break;
        case Mode::down1:
            for (int i = n - 1; i >= 0; --i) add (buffer[(std::size_t) i]);
            break;
        case Mode::downUp:
            for (int i = n - 1; i >= 0; --i) add (buffer[(std::size_t) i]);        // down: 3,2,1
            for (int i = 1; i < n - 1; ++i) add (buffer[(std::size_t) i]);         // up without end notes: 2
            break;
        case Mode::up2plus1:
            for (int i = 0; i < n; ++i) add (buffer[(std::size_t) i]);
            for (int i = 0; i < n; ++i) add (buffer[(std::size_t) i] + 12);
            break;
        case Mode::down2plus1:
            for (int i = n - 1; i >= 0; --i) add (buffer[(std::size_t) i]);
            for (int i = n - 1; i >= 0; --i) add (buffer[(std::size_t) i] + 12);
            break;
        case Mode::up3minus1:
            for (int i = 0; i < n; ++i) add (buffer[(std::size_t) i]);
            for (int i = 0; i < n; ++i) add (buffer[(std::size_t) i] - 12);
            break;
        case Mode::down3minus1:
            for (int i = n - 1; i >= 0; --i) add (buffer[(std::size_t) i]);
            for (int i = n - 1; i >= 0; --i) add (buffer[(std::size_t) i] - 12);
            break;
    }
}

int Arpeggiator::nextNote()
{
    if (seqCount == 0) return -1;

    if (mode == Mode::random)
    {
        rng = rng * 1664525u + 1013904223u;               // LCG
        return seq[(std::size_t) ((rng >> 16) % (unsigned) seqCount)];
    }

    const int note = seq[(std::size_t) (stepIndex % seqCount)];
    stepIndex = (stepIndex + 1) % seqCount;
    return note;
}
