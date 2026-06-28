#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <optional>

#include "Pattern.h"

// 128-slot pattern library, backed by ONE global XML file in app-data
// (shared by standalone and plugin, independent of any DAW project — like the
// fixed 128 banks of the hardware). Slots are 1-based (1..128); an empty slot is
// absent. The working/edited pattern travels separately in the plugin state.
class PatternBank
{
public:
    static constexpr int kNumSlots = 128;
    static constexpr int kVersion  = 1;     // schema version of the bank file

    void setFile (const juce::File& f) { file = f; }
    juce::File getFile() const         { return file; }

    void load();          // read the file → slots (logs a warning + empty bank on errors)
    void save() const;    // write slots → file (creates the directory)

    bool    has (int slot) const { return inRange (slot) && slots[(size_t) slot].has_value(); }
    Pattern get (int slot) const;                 // empty default pattern (with id=slot) if absent
    void    put (int slot, const Pattern& p)      { if (inRange (slot)) slots[(size_t) slot] = p; }
    void    clear (int slot)                      { if (inRange (slot)) slots[(size_t) slot].reset(); }

    int  lastSlot() const            { return lastPatternId; }
    void setLastSlot (int s)         { if (inRange (s)) lastPatternId = s; }
    int  countUsed() const;

private:
    static bool inRange (int s) { return s >= 1 && s <= kNumSlots; }

    juce::File file;
    std::array<std::optional<Pattern>, kNumSlots + 1> slots {};   // index 1..128
    int lastPatternId = 1;
};
