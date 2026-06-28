#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Global, project-independent settings. Like the
// 128 banks, these belong to the device, not to a DAW project → stored in
// one global file (settings.xml) next to banks.xml in app-data.
//
// Forwarding is OFF by default: the plugin then just works as a synth without
// surprising MIDI output; you enable it per source via the settings overlay.
struct GlobalSettings
{
    bool waitForPatternEnd = true;     // load pattern only at the pattern boundary

    // MIDI-in
    int  inputChannel   = 0;           // 0 = all channels, otherwise 1..16
    int  inputTranspose = 0;           // -12..+12 semitones on incoming notes
    int  accentThreshold = 96;         // velocity > threshold → accent (0..127)
    bool localControl   = true;        // false → keys only send MIDI, do not play the local voice
    int  keyPriority    = 0;           // 0 = low (303 default), 1 = high, 2 = last

    // MIDI-out
    int  outputChannel  = 1;           // 1..16
    bool fwdKeyboard    = false;        // key/host notes → MIDI-out
    bool fwdSequencer   = false;        // sequencer notes → MIDI-out
    bool fwdArp         = false;        // arp notes → MIDI-out
    bool fwdClock       = false;        // 24 PPQN MIDI clock + start/stop → MIDI-out

    // Clock-sync
    int  clockSource    = 0;           // 0 = internal, 1 = midi, 2 = auto
};

constexpr int kSettingsVersion = 1;

inline juce::ValueTree settingsToValueTree (const GlobalSettings& s)
{
    juce::ValueTree t ("SETTINGS");
    t.setProperty ("version",          kSettingsVersion,    nullptr);
    t.setProperty ("waitForPatternEnd", s.waitForPatternEnd, nullptr);
    t.setProperty ("inputChannel",     s.inputChannel,      nullptr);
    t.setProperty ("inputTranspose",   s.inputTranspose,    nullptr);
    t.setProperty ("accentThreshold",  s.accentThreshold,   nullptr);
    t.setProperty ("localControl",     s.localControl,      nullptr);
    t.setProperty ("keyPriority",      s.keyPriority,       nullptr);
    t.setProperty ("outputChannel",    s.outputChannel,     nullptr);
    t.setProperty ("fwdKeyboard",      s.fwdKeyboard,       nullptr);
    t.setProperty ("fwdSequencer",     s.fwdSequencer,      nullptr);
    t.setProperty ("fwdArp",           s.fwdArp,            nullptr);
    t.setProperty ("fwdClock",         s.fwdClock,          nullptr);
    t.setProperty ("clockSource",      s.clockSource,       nullptr);
    return t;
}

inline GlobalSettings settingsFromValueTree (const juce::ValueTree& t)
{
    GlobalSettings s;
    if (! t.hasType ("SETTINGS")) return s;
    s.waitForPatternEnd = (bool) t.getProperty ("waitForPatternEnd", s.waitForPatternEnd);
    s.inputChannel      = juce::jlimit (0, 16,    (int) t.getProperty ("inputChannel",    s.inputChannel));
    s.inputTranspose    = juce::jlimit (-12, 12,  (int) t.getProperty ("inputTranspose",  s.inputTranspose));
    s.accentThreshold   = juce::jlimit (0, 127,   (int) t.getProperty ("accentThreshold", s.accentThreshold));
    s.localControl      = (bool) t.getProperty ("localControl",     s.localControl);
    s.keyPriority       = juce::jlimit (0, 2,     (int) t.getProperty ("keyPriority",     s.keyPriority));
    s.outputChannel     = juce::jlimit (1, 16,    (int) t.getProperty ("outputChannel",   s.outputChannel));
    s.fwdKeyboard       = (bool) t.getProperty ("fwdKeyboard",      s.fwdKeyboard);
    s.fwdSequencer      = (bool) t.getProperty ("fwdSequencer",     s.fwdSequencer);
    s.fwdArp            = (bool) t.getProperty ("fwdArp",           s.fwdArp);
    s.fwdClock          = (bool) t.getProperty ("fwdClock",         s.fwdClock);
    s.clockSource       = juce::jlimit (0, 2,     (int) t.getProperty ("clockSource",     s.clockSource));
    return s;
}
