#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Open303Voice.h"
#include "dsp/VoiceAllocator.h"
#include "dsp/Saturation.h"
#include "dsp/Delay.h"
#include "sequencer/Pattern.h"
#include "sequencer/StepSequencer.h"
#include "sequencer/PatternCommand.h"
#include "sequencer/PatternBank.h"
#include "GlobalSettings.h"

#include <array>
#include <atomic>

// BASS SEQ — monophonic acid/bassline synth around Robin Schmidt's Open303.
// Signal chain:  Open303 → Saturation → ┬→ Master (dry)
//                                       └→ Delay → Master (wet)
class BassSeqProcessor : public juce::AudioProcessor
{
public:
    BassSeqProcessor();
    ~BassSeqProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return "BASS SEQ"; }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return true; }   // forwarding + MIDI-clock-out
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 3.0; }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // The editor writes notes here (on-screen / computer keyboard).
    juce::MidiKeyboardState keyboardState;

    // For the UI (read-only; editor/timer thread).
    int  getCurrentStep() const { return sequencer.getCurrentStep(); }
    bool isSequencerPlaying() const { return sequencer.isPlaying(); }
    Step getStepForDisplay (int index) const
    {
        const juce::SpinLock::ScopedLockType sl (uiSyncLock);
        syncUiPattern();
        return (index >= 0 && index < 16) ? uiPattern.steps[(size_t) index] : Step{};
    }
    int  getPatternLength() const { const juce::SpinLock::ScopedLockType sl (uiSyncLock); syncUiPattern(); return uiPattern.length; }
    int  getTranspose() const     { const juce::SpinLock::ScopedLockType sl (uiSyncLock); syncUiPattern(); return uiPattern.transpose; }

    // The editor posts edit commands here; the audio thread applies them as the sole
    // writer of the Pattern (lock-free, no torn reads during playback).
    void postPatternCommand (const PatternCommand& c);

    // --- Pattern memory (128 global banks) -------------------------
    int     getCurrentSlot() const          { return currentSlot; }
    bool    isSlotUsed (int slot) const     { return bank.has (slot); }
    Pattern getWorkingPatternCopy() const   { const juce::SpinLock::ScopedLockType sl (uiSyncLock); syncUiPattern(); return uiPattern; }
    void    saveWorkingToSlot (int slot);   // message thread: working pattern → slot + file
    void    loadSlot (int slot);            // message thread: slot → working pattern (optionally at pattern boundary)
    void    revertToCurrentSlot()           { loadSlot (currentSlot); }
    void    setBankFile (const juce::File& f);   // override/test hook
    bool    getWaitForPatternEnd() const    { return settings.waitForPatternEnd; }
    void    setWaitForPatternEnd (bool b)   { settings.waitForPatternEnd = b; saveSettings(); pushSettings(); }

    // --- Global settings (MIDI channels, forwarding, clock sync) ---
    GlobalSettings getSettings() const      { return settings; }
    void           setSettings (const GlobalSettings& s) { settings = s; saveSettings(); pushSettings(); }
    void           setSettingsFile (const juce::File& f);   // override/test hook
    double         getClockBpm() const      { return clockBpm.load(); }   // derived from MIDI clock (test/UI)

    // --- Live performance overrides (UI thread writes; audio thread reads) ---
    void setLiveTranspose (int v)  { liveTranspose.store (v); }
    void setLiveHold (bool b)      { liveHold.store (b); }
    void setLiveAccent (bool b)    { liveAccent.store (b); }
    void setLiveSlide (bool b)     { liveSlide.store (b); }
    void setLiveRatchet (int v)    { liveRatchet.store (v); }   // 0 = off
    void setLiveGate (int v)       { liveGate.store (v); }      // -1 = off
    void setLiveMute (bool b)      { liveMute.store (b); }

    int  getLiveTranspose() const  { return liveTranspose.load(); }
    bool getLiveHold() const       { return liveHold.load(); }
    bool getLiveAccent() const     { return liveAccent.load(); }
    bool getLiveSlide() const      { return liveSlide.load(); }
    int  getLiveRatchet() const    { return liveRatchet.load(); }
    int  getLiveGate() const       { return liveGate.load(); }
    bool getLiveMute() const       { return liveMute.load(); }

    // --- Arpeggiator (UI thread writes the parameters) ---------------
    void setArpActive (bool b)     { arpActive.store (b); }
    void setArpMode (int m)        { arpMode.store (m); }      // 1..8
    void setArpGate (int g)        { arpGate.store (g); }      // 1..8
    void setArpAccent (bool b)     { arpAccent.store (b); }
    void setArpHold (bool b)       { arpHold.store (b); }
    bool getArpActive() const      { return arpActive.load(); }
    int  getArpMode() const        { return arpMode.load(); }
    int  getArpGate() const        { return arpGate.load(); }
    bool getArpAccent() const      { return arpAccent.load(); }
    bool getArpHold() const        { return arpHold.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushParametersToDsp();

    // Fast, lock-free pointers to the parameter values.
    std::atomic<float>* pWave   = nullptr;
    std::atomic<float>* pPitch  = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pReso   = nullptr;
    std::atomic<float>* pEnvMod = nullptr;
    std::atomic<float>* pDecay  = nullptr;
    std::atomic<float>* pAccent = nullptr;
    std::atomic<float>* pSatOn  = nullptr;
    std::atomic<float>* pDrive  = nullptr;
    std::atomic<float>* pTone   = nullptr;
    std::atomic<float>* pDlyOn  = nullptr;
    std::atomic<float>* pDlyLvl = nullptr;
    std::atomic<float>* pDlyTime= nullptr;
    std::atomic<float>* pDlyFb  = nullptr;
    std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pPlay   = nullptr;
    std::atomic<float>* pTempo  = nullptr;

    Open303Voice    voice;
    VoiceAllocator  allocator;
    Saturation      saturation;
    FeedbackDelay   delay;
    juce::SmoothedValue<float> masterGain { 0.7f };

    Pattern        pattern = makeTestPattern();   // audio-thread copy (sole writer = audio thread)
    StepSequencer  sequencer;

    // Lock-free edit-command FIFO (UI → audio). The audio thread drains it at the
    // start of each block and is therefore the sole mutator of `pattern`.
    static constexpr int kCmdCapacity = 512;
    juce::AbstractFifo               cmdFifo { kCmdCapacity };
    std::array<PatternCommand, kCmdCapacity> cmdBuffer;
    void drainPatternCommands();

    // Audio -> UI snapshot of the pattern. The audio thread is the sole writer of
    // `pattern`; whenever it changes (edit/load/boundary) it publishes a copy that the
    // message thread reads as `uiPattern` for display/save. Lock-free SPSC, no race.
    mutable Pattern                      uiPattern = makeTestPattern();
    static constexpr int kUiSyncCapacity = 8;
    mutable juce::AbstractFifo           uiSyncFifo { kUiSyncCapacity };
    std::array<Pattern, kUiSyncCapacity> uiSyncBuf;
    mutable juce::SpinLock               uiSyncLock;   // serialises the non-audio consumers (editor + host save)
    bool patternDirty = false;           // audio thread: `pattern` changed this block
    void publishUiPattern();             // audio thread: push a snapshot if it changed (lock-free producer)
    void syncUiPattern() const;          // consumer side: adopt the latest snapshot (call under uiSyncLock)

    // --- Bank + pattern-load handoff (message → audio) -------------
    PatternBank bank;
    int  currentSlot       = 1;

    // Global settings (incl. waitForPatternEnd) — separate global file.
    // `settings` is the message-thread copy (UI reads/writes); the audio thread reads
    // its own `audioSettings`, refreshed from a lock-free FIFO at the top of each block.
    GlobalSettings settings;
    GlobalSettings audioSettings;
    juce::File     settingsFile;
    void loadSettings();
    void saveSettings();

    static constexpr int kSettingsCapacity = 8;
    juce::AbstractFifo                            settingsFifo { kSettingsCapacity };
    std::array<GlobalSettings, kSettingsCapacity> settingsBuf;
    void pushSettings();    // message thread: queue the current settings for the audio thread
    void drainSettings();   // audio thread: adopt the latest queued settings

    // Lock-free pattern-load handoff (message -> audio). A small SPSC FIFO so several
    // rapid loads / state-restores can never race over a single shared staging buffer.
    struct StagedLoad { Pattern pattern; bool defer = false; };
    static constexpr int kLoadCapacity = 8;
    juce::AbstractFifo                    loadFifo { kLoadCapacity };
    std::array<StagedLoad, kLoadCapacity> loadBuf;
    Pattern deferredPattern;           // audio thread: waits for the pattern boundary
    bool    hasDeferred = false;
    void requestLoad (const Pattern& p, bool defer);   // message thread
    void applyStagedLoad();                             // audio thread (drains loadFifo)

    // Live overrides: UI writes atomics, audio thread reads them each block into the sequencer.
    std::atomic<int>  liveTranspose { 0 };
    std::atomic<bool> liveHold   { false };
    std::atomic<bool> liveAccent { false };
    std::atomic<bool> liveSlide  { false };
    std::atomic<int>  liveRatchet { 0 };
    std::atomic<int>  liveGate   { -1 };
    std::atomic<bool> liveMute   { false };
    void pushLiveOverrides();                           // audio thread

    // Arp: parameters via atomics, the Arpeggiator object on the audio thread.
    Arpeggiator       arp;
    std::atomic<bool> arpActive { false };
    std::atomic<int>  arpMode   { 1 };
    std::atomic<int>  arpGate   { 8 };
    std::atomic<bool> arpAccent { false };
    std::atomic<bool> arpHold   { false };
    bool              prevArpActive = false;
    void pushArpParams();                               // audio thread

    // --- MIDI routing (audio thread) ----------------------------------
    juce::MidiBuffer midiOut;          // generated MIDI output (forwarding + clock)
    int    midiOutNote    = -1;        // note currently "on" in the MIDI-out stream (mono mirror)
    int    midiOutChannel = 1;         // channel that note started on → note-off on the same channel
    int    curSample      = 0;         // current sample position within the block
    int    outChannelNow  = 1;         // output channel for this block
    VoiceAllocator::Priority allocPriority = VoiceAllocator::Priority::low;
    void   midiVoiceOn (int note, bool accent, bool forward);
    void   midiVoiceOff();
    void   handleNoteIn (const juce::MidiMessage& msg, const GlobalSettings& st, bool arpOn);
    void   handleNoteOff (const juce::MidiMessage& msg, const GlobalSettings& st, bool arpOn);

    // Clock sync
    double sampleRateHz       = 44100.0;
    double samplesPerClock    = 0.0;   // 24 PPQN = samplesPerStep/6
    double samplesToNextClock = 0.0;
    double totalSamples       = 0.0;   // running sample counter (clock-in timing)
    double lastClockSample    = -1.0;  // sample of the previous incoming 0xF8
    double lastClockRecvSample = -1.0e18;  // for auto-revert to internal
    std::atomic<double> clockBpm { 0.0 };   // tempo derived from incoming MIDI clock (audio writes, UI reads)
    bool   midiTransportRunning = false;   // via MIDI Start/Stop (slave mode)
    void   handleClockIn (const juce::MidiMessage& msg);

    juce::AudioBuffer<float> monoBuffer;   // mono scratch (Open303 is monophonic)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassSeqProcessor)
};
