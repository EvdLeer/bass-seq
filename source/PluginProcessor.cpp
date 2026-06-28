#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "sequencer/PatternStorage.h"

#include <cmath>   // std::isfinite

//==============================================================================
BassSeqProcessor::BassSeqProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pWave    = apvts.getRawParameterValue ("WAVE");
    pPitch   = apvts.getRawParameterValue ("PITCH");
    pCutoff  = apvts.getRawParameterValue ("CUTOFF");
    pReso    = apvts.getRawParameterValue ("RESONANCE");
    pEnvMod  = apvts.getRawParameterValue ("ENVMOD");
    pDecay   = apvts.getRawParameterValue ("DECAY");
    pAccent  = apvts.getRawParameterValue ("ACCENT");
    pSatOn   = apvts.getRawParameterValue ("SAT_ON");
    pDrive   = apvts.getRawParameterValue ("DRIVE");
    pTone    = apvts.getRawParameterValue ("TONE");
    pDlyOn   = apvts.getRawParameterValue ("DLY_ON");
    pDlyLvl  = apvts.getRawParameterValue ("DLY_LEVEL");
    pDlyTime = apvts.getRawParameterValue ("DLY_TIME");
    pDlyFb   = apvts.getRawParameterValue ("DLY_FB");
    pMaster  = apvts.getRawParameterValue ("MASTER");
    pPlay    = apvts.getRawParameterValue ("PLAY");
    pTempo   = apvts.getRawParameterValue ("TEMPO");

    // The allocator drives the mono voice; on note change no noteOff -> slide.
    // We also mirror the voice to MIDI out (forwarding from the keyboard).
    allocator.onNoteOn  = [this] (int midi, bool accent) { voice.noteOn (midi, accent); midiVoiceOn (midi, accent, audioSettings.fwdKeyboard); };
    allocator.onNoteOff = [this] ()                      { voice.noteOff(); midiVoiceOff(); };

    // The sequencer drives the same mono voice; mirror to MIDI out (sequencer or arp).
    sequencer.onNoteOn  = [this] (int midi, bool accent) { voice.noteOn (midi, accent);
                              midiVoiceOn (midi, accent, arpActive.load() ? audioSettings.fwdArp : audioSettings.fwdSequencer); };
    sequencer.onNoteOff = [this] ()                      { voice.noteOff(); midiVoiceOff(); };
    sequencer.setPattern (&pattern);

    // A deferred pattern load is applied cleanly at the pattern boundary (audio thread).
    sequencer.onPatternWrap = [this] { if (hasDeferred) { pattern = deferredPattern; hasDeferred = false; patternDirty = true; } };

    // Arp: the sequencer queries this object as note source when arp is active.
    sequencer.setArp (&arp);

    // Load the global bank and take the last-used slot as the working pattern.
    const auto bankDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("BASS SEQ");
    bank.setFile (bankDir.getChildFile ("banks.xml"));
    bank.load();
    currentSlot = bank.lastSlot();
    if (bank.has (currentSlot))
        pattern = bank.get (currentSlot);
    uiPattern = pattern;                 // shadow starts in sync (before the audio thread runs)

    // Global settings from the same app-data folder.
    settingsFile = bankDir.getChildFile ("settings.xml");
    loadSettings();
    audioSettings = settings;            // audio copy starts in sync (before the audio thread runs)
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BassSeqProcessor::createParameterLayout()
{
    using Float  = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool   = juce::AudioParameterBool;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    const juce::NormalisableRange<float> unit { 0.0f, 1.0f, 0.001f };

    p.push_back (std::make_unique<Choice> (juce::ParameterID { "WAVE", 1 }, "Wave",
                                           juce::StringArray { "Saw", "Square" }, 0));
    p.push_back (std::make_unique<Float>  (juce::ParameterID { "PITCH", 1 }, "Pitch",
                                           juce::NormalisableRange<float> { -7.0f, 7.0f, 1.0f }, 0.0f));

    p.push_back (std::make_unique<Float> (juce::ParameterID { "CUTOFF",    1 }, "Cutoff",    unit, 0.40f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "RESONANCE", 1 }, "Resonance", unit, 0.40f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "ENVMOD",    1 }, "Env Mod",   unit, 0.60f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "DECAY",     1 }, "Decay",     unit, 0.30f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "ACCENT",    1 }, "Accent",    unit, 0.50f));

    p.push_back (std::make_unique<Bool>  (juce::ParameterID { "SAT_ON", 1 }, "Saturation", false));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "DRIVE",  1 }, "Drive", unit, 0.40f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "TONE",   1 }, "Tone",  unit, 0.50f));

    p.push_back (std::make_unique<Bool>  (juce::ParameterID { "DLY_ON",    1 }, "Delay",    false));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "DLY_LEVEL", 1 }, "Dly Level", unit, 0.40f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "DLY_TIME",  1 }, "Dly Time",  unit, 0.35f));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "DLY_FB",    1 }, "Dly Fdbk",  unit, 0.30f));

    p.push_back (std::make_unique<Float> (juce::ParameterID { "MASTER", 1 }, "Master", unit, 0.70f));

    // Transport via the PLAY/TEMPO parameters (also from the panel buttons).
    p.push_back (std::make_unique<Bool>  (juce::ParameterID { "PLAY", 1 }, "Play", false));
    p.push_back (std::make_unique<Float> (juce::ParameterID { "TEMPO", 1 }, "Tempo",
                                          juce::NormalisableRange<float> { 40.0f, 240.0f, 1.0f }, 120.0f));

    return { p.begin(), p.end() };
}

//==============================================================================
void BassSeqProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    totalSamples = 0.0;
    lastClockSample = -1.0;
    lastClockRecvSample = -1.0e18;
    voice.prepare (sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 1;

    saturation.prepare (spec);
    delay.prepare (spec);

    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (pMaster->load());

    sequencer.prepare (sampleRate);
    sequencer.setTempo (pTempo->load());

    monoBuffer.setSize (1, samplesPerBlock, false, false, true);
}

bool BassSeqProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void BassSeqProcessor::pushParametersToDsp()
{
    voice.setWaveformSquare (pWave->load() > 0.5f);
    voice.setPitchOffsetSemitones (pPitch->load());
    voice.setCutoffKnob (pCutoff->load());
    voice.setResonanceKnob (pReso->load());
    voice.setEnvModKnob (pEnvMod->load());
    voice.setDecayKnob (pDecay->load());
    voice.setAccentKnob (pAccent->load());

    saturation.setEnabled (pSatOn->load() > 0.5f);
    saturation.setDriveKnob (pDrive->load());
    saturation.setToneKnob (pTone->load());

    delay.setEnabled (pDlyOn->load() > 0.5f);
    delay.setLevelKnob (pDlyLvl->load());
    delay.setTimeKnob (pDlyTime->load());
    delay.setFeedbackKnob (pDlyFb->load());

    masterGain.setTargetValue (pMaster->load());
}

void BassSeqProcessor::postPatternCommand (const PatternCommand& c)
{
    int start1, size1, start2, size2;
    cmdFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)      cmdBuffer[(size_t) start1] = c;
    else if (size2 > 0) cmdBuffer[(size_t) start2] = c;
    cmdFifo.finishedWrite (size1 + size2);   // on a full FIFO: 0 -> command is dropped (rare at click speed)
}

void BassSeqProcessor::drainPatternCommands()
{
    int start1, size1, start2, size2;
    cmdFifo.prepareToRead (cmdFifo.getNumReady(), start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i) applyPatternCommand (pattern, cmdBuffer[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) applyPatternCommand (pattern, cmdBuffer[(size_t) (start2 + i)]);
    cmdFifo.finishedRead (size1 + size2);
    if (size1 + size2 > 0) patternDirty = true;   // changed -> republish to the UI
}

void BassSeqProcessor::publishUiPattern()
{
    if (! patternDirty) return;
    int start1, size1, start2, size2;
    uiSyncFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)      uiSyncBuf[(size_t) start1] = pattern;
    else if (size2 > 0) uiSyncBuf[(size_t) start2] = pattern;
    uiSyncFifo.finishedWrite (size1 + size2);
    if (size1 + size2 > 0) patternDirty = false;   // keep dirty if the FIFO was full -> retry next block
}

void BassSeqProcessor::syncUiPattern() const
{
    int start1, size1, start2, size2;
    uiSyncFifo.prepareToRead (uiSyncFifo.getNumReady(), start1, size1, start2, size2);
    if (size2 > 0)      uiPattern = uiSyncBuf[(size_t) (start2 + size2 - 1)];   // latest snapshot wins
    else if (size1 > 0) uiPattern = uiSyncBuf[(size_t) (start1 + size1 - 1)];
    uiSyncFifo.finishedRead (size1 + size2);
}

//==============================================================================
void BassSeqProcessor::requestLoad (const Pattern& p, bool defer)
{
    int start1, size1, start2, size2;
    loadFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)      loadBuf[(size_t) start1] = { p, defer };
    else if (size2 > 0) loadBuf[(size_t) start2] = { p, defer };
    loadFifo.finishedWrite (size1 + size2);
}

void BassSeqProcessor::pushLiveOverrides()
{
    StepSequencer::LiveOverrides lo;
    lo.transpose = liveTranspose.load();
    lo.hold      = liveHold.load();
    lo.accent    = liveAccent.load();
    lo.slide     = liveSlide.load();
    lo.ratchet   = juce::jlimit (0, 4, liveRatchet.load());   // bound the ratchet loop (defense in depth)
    lo.gate      = juce::jlimit (-1, 8, liveGate.load());
    lo.mute      = liveMute.load();
    sequencer.setLive (lo);
}

void BassSeqProcessor::pushArpParams()
{
    const bool active = arpActive.load();
    if (active != prevArpActive)        // arp on/off -> hand notes over cleanly
    {
        if (active) allocator.releaseAll();   // into arp: release directly-played notes
        else        arp.clear();              // out of arp: clear the chord buffer
        prevArpActive = active;
    }
    arp.setMode (arpMode.load());
    arp.setHold (arpHold.load());
    sequencer.setArpParams (active, arpGate.load(), arpAccent.load());
}

void BassSeqProcessor::applyStagedLoad()
{
    int start1, size1, start2, size2;
    loadFifo.prepareToRead (loadFifo.getNumReady(), start1, size1, start2, size2);

    auto apply = [this] (const StagedLoad& sl)
    {
        if (sl.defer && sequencer.isPlaying())
        {
            deferredPattern = sl.pattern;   // apply only on the next pattern boundary
            hasDeferred     = true;
        }
        else
        {
            pattern      = sl.pattern;       // apply immediately (stopped or no wait-for-end)
            hasDeferred  = false;
            patternDirty = true;             // changed now -> republish to the UI
        }
    };

    for (int i = 0; i < size1; ++i) apply (loadBuf[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) apply (loadBuf[(size_t) (start2 + i)]);
    loadFifo.finishedRead (size1 + size2);
}

void BassSeqProcessor::pushSettings()
{
    int start1, size1, start2, size2;
    settingsFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)      settingsBuf[(size_t) start1] = settings;
    else if (size2 > 0) settingsBuf[(size_t) start2] = settings;
    settingsFifo.finishedWrite (size1 + size2);
}

void BassSeqProcessor::drainSettings()
{
    int start1, size1, start2, size2;
    settingsFifo.prepareToRead (settingsFifo.getNumReady(), start1, size1, start2, size2);
    // Latest queued settings win; older queued entries are superseded.
    if (size2 > 0)      audioSettings = settingsBuf[(size_t) (start2 + size2 - 1)];
    else if (size1 > 0) audioSettings = settingsBuf[(size_t) (start1 + size1 - 1)];
    settingsFifo.finishedRead (size1 + size2);
}

void BassSeqProcessor::setBankFile (const juce::File& f)
{
    bank.setFile (f);
    bank.load();
    currentSlot = bank.lastSlot();
    if (bank.has (currentSlot))
        requestLoad (bank.get (currentSlot), false);
}

void BassSeqProcessor::saveWorkingToSlot (int slot)
{
    Pattern p = getWorkingPatternCopy();    // race-free snapshot (synced from the audio thread)
    p.id    = slot;
    p.tempo = (double) pTempo->load();      // store the current tempo as well
    bank.put (slot, p);
    currentSlot = slot;
    bank.setLastSlot (slot);
    bank.save();
}

void BassSeqProcessor::loadSlot (int slot)
{
    const Pattern p = bank.get (slot);
    currentSlot = slot;
    bank.setLastSlot (slot);
    bank.save();    // also record the last-LOADED slot in the global file

    // Take over the loaded pattern's tempo onto the TEMPO parameter.
    if (auto* prm = apvts.getParameter ("TEMPO"))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (40.0, 240.0, p.tempo)));

    requestLoad (p, settings.waitForPatternEnd);
}

//==============================================================================
void BassSeqProcessor::loadSettings()
{
    if (settingsFile == juce::File() || ! settingsFile.existsAsFile())
        return;
    auto xml = juce::XmlDocument::parse (settingsFile);
    if (xml == nullptr)
    {
        juce::Logger::writeToLog ("Settings: could not parse " + settingsFile.getFullPathName()
                                  + " — keeping defaults.");
        return;
    }
    const auto tree = juce::ValueTree::fromXml (*xml);
    const int ver = (int) tree.getProperty ("version", kSettingsVersion);
    if (ver > kSettingsVersion)   // newer than this build -> don't interpret, use defaults
    {
        juce::Logger::writeToLog ("Settings: version " + juce::String (ver)
                                  + " newer than " + juce::String (kSettingsVersion) + " — keeping defaults.");
        return;
    }
    settings = settingsFromValueTree (tree);
}

void BassSeqProcessor::saveSettings()
{
    if (settingsFile == juce::File())
        return;
    settingsFile.getParentDirectory().createDirectory();
    if (auto xml = settingsToValueTree (settings).createXml())
        xml->writeTo (settingsFile);
}

void BassSeqProcessor::setSettingsFile (const juce::File& f)
{
    settingsFile = f;
    loadSettings();
    audioSettings = settings;   // test/override hook (called before audio runs)
    pushSettings();             // …and queue for the audio thread if it is running
}

//==============================================================================
void BassSeqProcessor::midiVoiceOn (int note, bool accent, bool forward)
{
    if (midiOutNote >= 0)   // mono mirror: first release the previous note (on ITS channel)
    {
        midiOut.addEvent (juce::MidiMessage::noteOff (midiOutChannel, midiOutNote), curSample);
        midiOutNote = -1;
    }
    if (forward && note >= 0 && note <= 127)
    {
        midiOutChannel = outChannelNow;
        midiOut.addEvent (juce::MidiMessage::noteOn (midiOutChannel, note, (juce::uint8) (accent ? 127 : 100)), curSample);
        midiOutNote = note;
    }
}

void BassSeqProcessor::midiVoiceOff()
{
    if (midiOutNote >= 0)   // note-off on the channel the note started on (not the live channel)
    {
        midiOut.addEvent (juce::MidiMessage::noteOff (midiOutChannel, midiOutNote), curSample);
        midiOutNote = -1;
    }
}

void BassSeqProcessor::handleNoteIn (const juce::MidiMessage& msg, const GlobalSettings& st, bool arpOn)
{
    if (st.inputChannel != 0 && msg.getChannel() != st.inputChannel) return;   // channel filter (0 = all)
    const int note = msg.getNoteNumber() + st.inputTranspose;
    if (note < 0 || note > 127) return;
    const bool accent = msg.getVelocity() > st.accentThreshold;

    if (arpOn)                arp.noteOn (note);                        // -> chord buffer
    else if (st.localControl) allocator.noteOn (note, accent);         // voice + MIDI mirror via callback
    else                      midiVoiceOn (note, accent, st.fwdKeyboard);  // local control off -> MIDI only
}

void BassSeqProcessor::handleNoteOff (const juce::MidiMessage& msg, const GlobalSettings& st, bool arpOn)
{
    if (st.inputChannel != 0 && msg.getChannel() != st.inputChannel) return;
    const int note = msg.getNoteNumber() + st.inputTranspose;
    if (note < 0 || note > 127) return;

    if (arpOn)                arp.noteOff (note);
    else if (st.localControl) allocator.noteOff (note);
    else                      midiVoiceOff();
}

void BassSeqProcessor::handleClockIn (const juce::MidiMessage& msg)
{
    if (msg.isMidiClock())   // 0xF8 — derive tempo from the interval (24 PPQN)
    {
        const double absSample = totalSamples + curSample;
        if (lastClockSample >= 0.0)
        {
            const double interval = absSample - lastClockSample;
            if (interval > 1.0)
                clockBpm.store (juce::jlimit (20.0, 300.0, (sampleRateHz * 60.0) / (interval * 24.0)));
        }
        lastClockSample     = absSample;
        lastClockRecvSample = absSample;
    }
    else if (msg.isMidiStart() || msg.isMidiContinue()) midiTransportRunning = true;
    else if (msg.isMidiStop())                          midiTransportRunning = false;
}

void BassSeqProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    curSample = 0;
    midiOut.clear();            // output accumulator for this block (forwarding + clock)
    applyStagedLoad();          // apply a requested pattern load (immediately or at boundary)
    drainSettings();            // adopt the latest UI settings into the audio-thread copy
    drainPatternCommands();     // apply UI edits before the sequencer reads this block
    pushLiveOverrides();        // set live-performance overlays in the sequencer
    pushArpParams();            // arp parameters + on/off transition
    pushParametersToDsp();

    const GlobalSettings st = audioSettings;   // audio-thread copy (refreshed via drainSettings)
    outChannelNow = juce::jlimit (1, 16, st.outputChannel);

    // Note priority (low/high/last) from the settings -> allocator (only on change).
    const auto wantPrio = (VoiceAllocator::Priority) juce::jlimit (0, 2, st.keyPriority);
    if (wantPrio != allocPriority) { allocator.setPriority (wantPrio); allocPriority = wantPrio; }

    // Clock source: internal follows host BPM/param; midi/auto follows incoming MIDI clock.
    bool followMidi = (st.clockSource == 1);
    if (st.clockSource == 2)   // auto: follow MIDI while clock arrived recently (~500 ms)
        followMidi = (totalSamples - lastClockRecvSample) < 0.5 * sampleRateHz;

    double tempo = (double) pTempo->load();
    const double cb = clockBpm.load();
    if (followMidi && cb > 0.0)
        tempo = cb;
    else if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                tempo = *bpm;
    if (! std::isfinite (tempo)) tempo = 120.0;     // a misbehaving host BPM must not poison timing
    tempo = juce::jlimit (20.0, 300.0, tempo);
    sequencer.setTempo (tempo);
    samplesPerClock = sampleRateHz * (60.0 / tempo) / 24.0;   // 24 PPQN

    // Transport: internally via the PLAY parameter, in slave mode via MIDI Start/Stop.
    const bool wantPlay = followMidi ? midiTransportRunning : (pPlay->load() > 0.5f);
    if (wantPlay && ! sequencer.isPlaying())      { sequencer.start(); samplesToNextClock = 0.0;
                                                    if (st.fwdClock && ! followMidi) midiOut.addEvent (juce::MidiMessage::midiStart(), 0); }
    else if (! wantPlay && sequencer.isPlaying()) { sequencer.stop();
                                                    if (st.fwdClock && ! followMidi) midiOut.addEvent (juce::MidiMessage::midiStop(), 0); }

    // Mix notes from the on-screen / computer keyboard with the host MIDI.
    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    if (monoBuffer.getNumSamples() < numSamples)
        monoBuffer.setSize (1, numSamples, false, false, true);

    float* mono = monoBuffer.getWritePointer (0);

    const bool arpOn = arpActive.load();
    const bool emitClock = st.fwdClock && ! followMidi && sequencer.isPlaying();

    auto midiIt = midiMessages.cbegin();
    const auto midiEnd = midiMessages.cend();
    for (int i = 0; i < numSamples; ++i)
    {
        curSample = i;
        while (midiIt != midiEnd && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn() && msg.getVelocity() > 0)      handleNoteIn (msg, st, arpOn);
            else if (msg.isNoteOff() || msg.isNoteOn())       handleNoteOff (msg, st, arpOn);  // noteOn vel 0 = off
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                if (arpOn) arp.clear(); else allocator.releaseAll();
            }
            else if (st.clockSource != 0
                     && (msg.isMidiClock() || msg.isMidiStart() || msg.isMidiStop() || msg.isMidiContinue()))
                handleClockIn (msg);
            ++midiIt;
        }

        // MIDI clock out (24 PPQN) while the internal clock is master.
        if (emitClock)
        {
            samplesToNextClock -= 1.0;
            if (samplesToNextClock <= 0.0)
            {
                midiOut.addEvent (juce::MidiMessage::midiClock(), i);
                samplesToNextClock += samplesPerClock;
            }
        }

        sequencer.processSample();
        mono[i] = voice.getSample();
    }

    // FX chain on the mono signal.
    saturation.processMono (mono, numSamples);
    delay.processMonoAdd (mono, numSamples);

    for (int i = 0; i < numSamples; ++i)
        mono[i] *= masterGain.getNextValue();

    // Mono -> all output channels.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, monoBuffer, 0, 0, numSamples);

    // Our generated MIDI (forwarding + clock) becomes the output; incoming MIDI is consumed.
    midiMessages.swapWith (midiOut);
    totalSamples += numSamples;

    publishUiPattern();   // if `pattern` changed this block, hand a snapshot to the UI
}

//==============================================================================
juce::AudioProcessorEditor* BassSeqProcessor::createEditor()
{
    return new BassSeqEditor (*this);
}

void BassSeqProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Own root with (1) the APVTS parameters and (2) a session block with the
    // edited working pattern + the current slot. The bank itself is GLOBAL (separate file),
    // so it does not belong in the plugin state.
    juce::ValueTree root ("BASSSEQSTATE");
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree session ("SESSION");
    session.setProperty ("slot", currentSlot, nullptr);
    session.appendChild (patternToValueTree (getWorkingPatternCopy()), nullptr);
    root.appendChild (session, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void BassSeqProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto tree = juce::ValueTree::fromXml (*xml);

    if (tree.hasType ("BASSSEQSTATE"))
    {
        if (auto params = tree.getChildWithName ("PARAMETERS"); params.isValid())
            apvts.replaceState (params);

        if (auto session = tree.getChildWithName ("SESSION"); session.isValid())
        {
            currentSlot = juce::jlimit (1, PatternBank::kNumSlots,
                                        (int) session.getProperty ("slot", currentSlot));
            if (auto pt = session.getChildWithName ("PATTERN"); pt.isValid())
                requestLoad (patternFromValueTree (pt), false);   // safe via the audio thread
        }
    }
    else
    {
        apvts.replaceState (tree);   // old format (PARAMETERS only) — backward compatible
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassSeqProcessor();
}
