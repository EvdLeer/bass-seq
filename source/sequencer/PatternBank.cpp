#include "PatternBank.h"
#include "PatternStorage.h"

Pattern PatternBank::get (int slot) const
{
    if (has (slot)) return *slots[(size_t) slot];
    Pattern p;                       // empty slot → default pattern with this slot number as id
    p.id = inRange (slot) ? slot : 1;
    return p;
}

int PatternBank::countUsed() const
{
    int n = 0;
    for (int s = 1; s <= kNumSlots; ++s)
        if (has (s)) ++n;
    return n;
}

void PatternBank::load()
{
    slots = {};
    if (file == juce::File() || ! file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
    {
        juce::Logger::writeToLog ("PatternBank: could not parse " + file.getFullPathName()
                                  + " — falling back to empty bank.");
        return;
    }

    const auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType ("BASSSEQBANKS"))
    {
        juce::Logger::writeToLog ("PatternBank: unexpected root in " + file.getFullPathName()
                                  + " — falling back to empty bank.");
        return;
    }

    // Schema version: newer than this build → do not interpret, fall back to empty
    // (the user is running a newer version elsewhere). Older → migrate here later.
    const int ver = (int) tree.getProperty ("version", kVersion);
    if (ver > kVersion)
    {
        juce::Logger::writeToLog ("PatternBank: bank version " + juce::String (ver)
                                  + " is newer than " + juce::String (kVersion)
                                  + " — falling back to empty bank.");
        return;
    }

    lastPatternId = juce::jlimit (1, kNumSlots, (int) tree.getProperty ("lastPattern", 1));

    for (int c = 0; c < tree.getNumChildren(); ++c)
    {
        const auto pt = tree.getChild (c);
        if (! pt.hasType ("PATTERN")) continue;
        const int slot = (int) pt.getProperty ("slot", -1);
        if (inRange (slot))
            slots[(size_t) slot] = patternFromValueTree (pt);
    }
}

void PatternBank::save() const
{
    if (file == juce::File())
        return;

    juce::ValueTree tree ("BASSSEQBANKS");
    tree.setProperty ("version",     kVersion,      nullptr);
    tree.setProperty ("lastPattern", lastPatternId, nullptr);

    for (int s = 1; s <= kNumSlots; ++s)
    {
        if (! slots[(size_t) s].has_value()) continue;
        auto pt = patternToValueTree (*slots[(size_t) s]);
        pt.setProperty ("slot", s, nullptr);
        tree.appendChild (pt, nullptr);
    }

    file.getParentDirectory().createDirectory();
    if (auto xml = tree.createXml())
        xml->writeTo (file);
}
