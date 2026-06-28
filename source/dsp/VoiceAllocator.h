#pragma once

#include <algorithm>
#include <functional>
#include <vector>

// Mono note-priority allocator (low/high/last note-priority).
// Tracks the held notes and picks which one sounds according to the priority.
// When the winner changes, onNoteOn is called WITHOUT a preceding onNoteOff:
// this lets Open303 produce legato/slide between consecutive notes.
class VoiceAllocator
{
public:
    enum class Priority { low, high, last };

    std::function<void (int midi, bool accent)> onNoteOn;
    std::function<void ()>                      onNoteOff;

    void setPriority (Priority p) { priority = p; reevaluate(); }

    void noteOn (int midi, bool accent)
    {
        if (auto* h = find (midi))
        {
            h->accent = accent;
            h->order  = ++counter;
        }
        else
        {
            held.push_back ({ midi, accent, ++counter });
        }
        reevaluate();
    }

    void noteOff (int midi)
    {
        held.erase (std::remove_if (held.begin(), held.end(),
                                    [midi] (const HeldNote& h) { return h.midi == midi; }),
                    held.end());
        reevaluate();
    }

    void releaseAll() { held.clear(); reevaluate(); }

private:
    struct HeldNote { int midi; bool accent; long order; };

    HeldNote* find (int midi)
    {
        auto it = std::find_if (held.begin(), held.end(),
                                [midi] (const HeldNote& h) { return h.midi == midi; });
        return it != held.end() ? &(*it) : nullptr;
    }

    const HeldNote* pickWinner() const
    {
        if (held.empty()) return nullptr;
        const HeldNote* w = &held.front();
        for (const auto& h : held)
        {
            switch (priority)
            {
                case Priority::low:  if (h.midi  < w->midi)  w = &h; break;
                case Priority::high: if (h.midi  > w->midi)  w = &h; break;
                case Priority::last: if (h.order > w->order) w = &h; break;
            }
        }
        return w;
    }

    void reevaluate()
    {
        const HeldNote* w = pickWinner();
        if (w == nullptr)
        {
            if (current != -1)
            {
                if (onNoteOff) onNoteOff();
                current = -1;
            }
            return;
        }
        if (current != w->midi)
        {
            if (onNoteOn) onNoteOn (w->midi, w->accent);   // no noteOff → slide
            current = w->midi;
        }
    }

    std::vector<HeldNote> held;
    long     counter  = 0;
    int      current  = -1;
    Priority priority = Priority::low;
};
