#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/PitchCurve.h"


Processor::Processor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", AudioChannelSet::stereo(), true))
{
}

Processor::~Processor()
{
    stopTimer();
}

void Processor::prepareToPlay(double sr, int )
{
    sampleRate = sr;

    outputMidiBuffer.clear();
    if (midiEngine.getMode() == MidiEngine::Mode::MPE)
        midiEngine.sendMPEConfiguration(outputMidiBuffer);

    if (wrapperType == wrapperType_Standalone && virtualMidiOut == nullptr)
    {
        virtualMidiOut = juce::MidiOutput::createNewDevice("Fluid Pitch 2");
        if (virtualMidiOut != nullptr)
            startTimer(2);
    }
}

void Processor::releaseResources() {}

bool Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    return true;
}

void Processor::setPlaying(bool p)
{
    if (p == playing) return;
    playing = p;

    if (!playing)
    {
        midiPanicRequested.store(true);
        livePitchBendSemitones = 0.0f;
    }
}

void Processor::injectNoteOn(int noteNumber, float velocity)
{
    auto msg = MidiMessage::noteOn(1, noteNumber, velocity);
    const SpinLock::ScopedLockType lock(injectedMidiLock);
    injectedMidi.addEvent(msg, 0);
}

void Processor::injectNoteOff(int noteNumber)
{
    auto msg = MidiMessage::noteOff(1, noteNumber, 0.0f);
    const SpinLock::ScopedLockType lock(injectedMidiLock);
    injectedMidi.addEvent(msg, 0);
}

void Processor::injectPitchWheel(int value14bit)
{
    auto msg = MidiMessage::pitchWheel(1, value14bit);
    const SpinLock::ScopedLockType lock(injectedMidiLock);
    injectedMidi.addEvent(msg, 0);
}

void Processor::injectCC(int ccNumber, int value)
{
    auto msg = MidiMessage::controllerEvent(1, ccNumber, value);
    const SpinLock::ScopedLockType lock(injectedMidiLock);
    injectedMidi.addEvent(msg, 0);
}

Processor::LiveNoteState Processor::getLiveNoteState() const
{
    const SpinLock::ScopedLockType lock(liveStateLock);
    return liveState;
}

static float interpolateRecordedSamples(const std::vector<RecordedPitchSample>& samples, double beat)
{
    if (samples.empty()) return 0.0f;
    if (beat <= samples.front().beat) return samples.front().semitones;
    if (beat >= samples.back().beat) return samples.back().semitones;

    for (size_t j = 1; j < samples.size(); ++j)
    {
        if (samples[j].beat >= beat)
        {
            double frac = (beat - samples[j-1].beat) / (samples[j].beat - samples[j-1].beat);
            return samples[j-1].semitones + (float)frac * (samples[j].semitones - samples[j-1].semitones);
        }
    }
    return samples.back().semitones;
}

std::vector<Processor::RecordingPreviewNote> Processor::getRecordingPreview() const
{
    std::vector<RecordingPreviewNote> result;
    if (!recordingBuffer.isRecording()) return result;

    double recStartAbsBeat = recordingBuffer.getRecordStartBeat();
    double currentAbsBeat = (double)absoluteSamplePosition * (bpm / (60.0 * sampleRate));

    auto& globalPB = recordingBuffer.getPitchBends();
    auto& perNotePB = recordingBuffer.getPerNotePitchBends();
    auto& exSamples = recordingBuffer.getExpressionOffsets();

    for (auto& evt : recordingBuffer.getNoteEvents())
    {
        RecordingPreviewNote preview;
        preview.noteNumber = evt.noteNumber;
        preview.velocity = evt.velocity;

        preview.startBeat = (evt.beatOn - recStartAbsBeat) + recordStartBeatInSequence;

        double noteEndAbs = (evt.beatOff < 0.0) ? currentAbsBeat : evt.beatOff;

        if (evt.beatOff < 0.0)
        {
            preview.durationBeats = noteEndAbs - evt.beatOn;
            if (preview.durationBeats < 0.05) preview.durationBeats = 0.05;
        }
        else
        {
            preview.durationBeats = evt.beatOff - evt.beatOn;
        }

        const std::vector<RecordedPitchSample>* pbSource = &globalPB;
        auto perNoteIt = perNotePB.find(evt.id);
        if (perNoteIt != perNotePB.end() && !perNoteIt->second.empty())
            pbSource = &perNoteIt->second;

        double noteOnBeat = evt.beatOn;
        double dur = noteEndAbs - noteOnBeat;
        if (dur > 0.01)
        {
            int numPoints = jmin(60, jmax(2, (int)(dur * 30.0)));
            double step = dur / (double)(numPoints - 1);

            for (int i = 0; i < numPoints; ++i)
            {
                double t = noteOnBeat + i * step;
                float pbVal = interpolateRecordedSamples(*pbSource, t);
                float exVal = interpolateRecordedSamples(exSamples, t);

                float total = pbVal + exVal;
                if (std::abs(total) > 0.001f || !preview.pitchCurve.empty())
                    preview.pitchCurve.push_back({ i * step, total });
            }
        }

        result.push_back(preview);
    }

    return result;
}

void Processor::updateLiveState()
{
    const SpinLock::ScopedLockType lock(liveStateLock);
    liveState.activeNotes.clear();
    for (auto& ln : liveNotes)
        liveState.activeNotes.insert(ln.noteNumber);
    liveState.expressionActive = activeExpression.active;
    liveState.lastExpression = activeExpression.name;
}

void Processor::armRecording()
{
    if (wrapperType == wrapperType_Standalone)
    {
        if (!playing)
            setPlaying(true);
        startRecording();
        return;
    }

    if (playing)
    {
        startRecording();
    }
    else
    {
        recordArmed.store(true);
    }
}

void Processor::disarmRecording()
{
    recordArmed.store(false);
}

void Processor::startRecording()
{
    recordArmed.store(false);
    double beatAtNow = (double)absoluteSamplePosition * (bpm / (60.0 * sampleRate));
    recordStartBeatInSequence = playheadBeat;
    recordingBuffer.clear();
    recordingBuffer.startRecording(beatAtNow);
}

void Processor::stopRecording()
{
    recordEndBeatInSequence = playheadBeat;
    double beatAtNow = (double)absoluteSamplePosition * (bpm / (60.0 * sampleRate));

    recordingBuffer.stopRecording(beatAtNow);
    recordingStopPending.store(true);
    triggerAsyncUpdate();
}

void Processor::punchReplaceRange(double rangeStart, double rangeEnd)
{
    auto& allNotes = noteSequence.getAllNotes();
    std::vector<NoteData> notesToAdd;

    for (int i = (int)allNotes.size() - 1; i >= 0; --i)
    {
        auto& n = allNotes[(size_t)i];
        double nStart = n.startBeat;
        double nEnd = n.getEndBeat();

        if (nEnd <= rangeStart || nStart >= rangeEnd)
            continue;

        bool startsBeforeRange = nStart < rangeStart;
        bool endsAfterRange = nEnd > rangeEnd;

        if (startsBeforeRange && endsAfterRange)
        {
            NoteData rightPart;
            rightPart.noteNumber = n.noteNumber;
            rightPart.velocity = n.velocity;
            rightPart.startBeat = rangeEnd;
            rightPart.durationBeats = nEnd - rangeEnd;

            double cutTimeInNote = rangeEnd - nStart;
            for (auto& pt : n.pitchCurve)
            {
                if (pt.time >= cutTimeInNote)
                {
                    PitchPoint shifted = pt;
                    shifted.time -= cutTimeInNote;
                    rightPart.pitchCurve.push_back(shifted);
                }
            }

            notesToAdd.push_back(rightPart);

            n.durationBeats = rangeStart - nStart;
            n.pitchCurve.erase(
                std::remove_if(n.pitchCurve.begin(), n.pitchCurve.end(),
                    [&](const PitchPoint& pt) { return pt.time >= n.durationBeats; }),
                n.pitchCurve.end());
        }
        else if (startsBeforeRange)
        {
            n.durationBeats = rangeStart - nStart;
            n.pitchCurve.erase(
                std::remove_if(n.pitchCurve.begin(), n.pitchCurve.end(),
                    [&](const PitchPoint& pt) { return pt.time >= n.durationBeats; }),
                n.pitchCurve.end());
        }
        else if (endsAfterRange)
        {
            double trimAmount = rangeEnd - nStart;
            n.startBeat = rangeEnd;
            n.durationBeats = nEnd - rangeEnd;

            std::vector<PitchPoint> shifted;
            for (auto& pt : n.pitchCurve)
            {
                if (pt.time >= trimAmount)
                {
                    PitchPoint sp = pt;
                    sp.time -= trimAmount;
                    shifted.push_back(sp);
                }
            }
            n.pitchCurve = shifted;
        }
        else
        {
            allNotes.erase(allNotes.begin() + i);
        }
    }

    for (auto& n : notesToAdd)
        noteSequence.addNote(n);
}

void Processor::timerCallback()
{
    if (virtualMidiOut == nullptr) return;

    MidiBuffer snapshot;
    {
        const SpinLock::ScopedLockType lk(pendingVirtualMidiLock);
        snapshot.swapWith(pendingVirtualMidi);
    }
    if (snapshot.isEmpty()) return;

    for (const auto meta : snapshot)
        virtualMidiOut->sendMessageNow(meta.getMessage());
}

void Processor::handleAsyncUpdate()
{
    if (recordingStopPending.exchange(false))
    {
        if (onBeforeRecordingMerge)
            onBeforeRecordingMerge();

        auto recordedNotes = recordingBuffer.toNoteData();

        double recStart = recordStartBeatInSequence;
        double recEnd = recordEndBeatInSequence;

        for (auto& rn : recordedNotes)
            recEnd = std::max(recEnd, recStart + rn.startBeat + rn.durationBeats);

        {
            const SpinLock::ScopedLockType lock(sequenceLock);
            if (!overdubEnabled.load())
            {
                punchReplaceRange(recStart, recEnd);
            }

            for (auto& note : recordedNotes)
            {
                note.startBeat += recordStartBeatInSequence;
                noteSequence.addNote(note);
            }
        }

        recordingBuffer.clear();

        activeExpression.active = false;
        liveNotes.clear();

        if (onRecordingFinished)
            onRecordingFinished();
    }
}

float Processor::ActiveExpression::evaluate(double currentBeat) const
{
    if (!active) return 0.0f;
    double elapsed = currentBeat - triggerBeat;
    if (elapsed < 0.0 || elapsed >= durationBeats) return 0.0f;
    return PitchCurveInterpolator::interpolate(pattern, elapsed);
}

void Processor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();

    {
        const SpinLock::ScopedLockType lock(injectedMidiLock);
        if (!injectedMidi.isEmpty())
        {
            for (const auto metadata : injectedMidi)
                midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);
            injectedMidi.clear();
        }
    }

    bool hostControlled = false;
    if (dawSyncEnabled)
    {
        if (auto* hostPlayHead = getPlayHead())
        {
            auto posInfo = hostPlayHead->getPosition();
            if (posInfo.hasValue())
            {
                if (auto bpmOpt = posInfo->getBpm())
                    bpm = *bpmOpt;

                bool hostIsPlaying = posInfo->getIsPlaying();

                if (auto ppq = posInfo->getPpqPosition())
                {
                    if (hostIsPlaying)
                    {
                        double newPpq = *ppq;
                        if (hostWasPlaying)
                        {
                            double bps = bpm / (60.0 * sampleRate);
                            double expectedDrift = buffer.getNumSamples() * bps * 4.0;
                            if (std::abs(newPpq - playheadBeat) > expectedDrift)
                                midiPanicRequested.store(true);
                        }
                        playheadBeat = newPpq;
                        if (!hostWasPlaying)
                        {
                            playing = true;
                            midiPanicRequested.store(true);

                            if (recordArmed.load())
                                startRecording();
                        }
                        hostControlled = true;
                    }
                    else
                    {
                        if (hostWasPlaying)
                        {
                            playing = false;
                            midiPanicRequested.store(true);

                            if (recordingBuffer.isRecording())
                                stopRecording();

                            recordArmed.store(false);
                        }
                        playheadBeat = *ppq;
                        hostControlled = true;
                    }
                }

                hostWasPlaying = hostIsPlaying;
            }
        }
    }
    else
    {
        hostWasPlaying = false;
    }

    double beatsPerSample = bpm / (60.0 * sampleRate);

    outputMidiBuffer.clear();
    midiEngine.prepareBlock(buffer.getNumSamples());

    if (midiPanicRequested.exchange(false))
    {
        midiEngine.allNotesOff(outputMidiBuffer, 0);

        for (int ch = 1; ch <= 16; ++ch)
        {
            outputMidiBuffer.addEvent(MidiMessage::allNotesOff(ch), 0);
            outputMidiBuffer.addEvent(MidiMessage::allSoundOff(ch), 0);
            outputMidiBuffer.addEvent(MidiMessage::pitchWheel(ch, 8192), 0);
        }

        activeSeqNotes.clear();
        livePitchBendSemitones = 0.0f;
        needsNoteCatchUp = true;
    }

    processLiveMidi(midiMessages, beatsPerSample);
    processSequencePlayback(buffer, midiMessages, beatsPerSample, hostControlled);

    if (activeExpression.active)
    {
        bool isRec = recordingBuffer.isRecording();
        const int targetNote = activeExpression.targetNoteNumber;
        float lastSentBend = activeExpression.lastSentBend;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            double currentBeat = (double)(absoluteSamplePosition + i) * beatsPerSample;
            float pitchOffset = activeExpression.evaluate(currentBeat);
            float combinedBend = pitchOffset + livePitchBendSemitones;

            if (targetNote >= 0 && (i % 32 == 0))
            {
                if (std::abs(combinedBend - lastSentBend) > 0.0008f)
                {
                    midiEngine.pitchBend(targetNote, combinedBend, i, outputMidiBuffer);
                    lastSentBend = combinedBend;
                }
            }

            if (isRec && (i % 32 == 0) && std::abs(pitchOffset) > 0.0001f)
                recordingBuffer.expressionOffset(currentBeat, pitchOffset);
        }

        activeExpression.lastSentBend = lastSentBend;

        double blockEndBeat = (double)(absoluteSamplePosition + buffer.getNumSamples()) * beatsPerSample;
        if (blockEndBeat >= activeExpression.triggerBeat + activeExpression.durationBeats)
        {
            activeExpression.active = false;
            activeExpression.name = {};

            if (targetNote >= 0)
                midiEngine.pitchBend(targetNote, livePitchBendSemitones,
                                         jmax(0, buffer.getNumSamples() - 1),
                                         outputMidiBuffer);

            activeExpression.targetNoteNumber = -1;
            activeExpression.lastSentBend = 0.0f;

            if (isRec)
                recordingBuffer.expressionOffset(blockEndBeat, 0.0f);
        }
    }

    updateLiveState();

    if (virtualMidiOut != nullptr && !outputMidiBuffer.isEmpty())
    {
        const SpinLock::ScopedTryLockType lk(pendingVirtualMidiLock);
        if (lk.isLocked())
            pendingVirtualMidi.addEvents(outputMidiBuffer, 0, -1, 0);
    }

    midiMessages.swapWith(outputMidiBuffer);

    absoluteSamplePosition += buffer.getNumSamples();
}

void Processor::processLiveMidi(MidiBuffer& midiMessages, double beatsPerSample)
{
    MidiBuffer filteredMidi;
    bool recording = recordingBuffer.isRecording();

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        int samplePos = metadata.samplePosition;
        double beatAtSample = (double)(absoluteSamplePosition + samplePos) * beatsPerSample;

        int msgChannel = msg.getChannel();
        bool isMPEMemberChannel = (msgChannel >= 2 && msgChannel <= 16);

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();

            {
                auto synthMsg = MidiMessage::noteOn(1, note, msg.getFloatVelocity());
                filteredMidi.addEvent(synthMsg, samplePos);
                midiEngine.noteOn(note, msg.getFloatVelocity(), samplePos, outputMidiBuffer);

                LiveNote ln;
                ln.noteNumber = note;
                ln.velocity = msg.getFloatVelocity();
                ln.startBeat = beatAtSample;
                ln.midiChannel = msgChannel;
                ln.recordingId = -1;

                if (recording)
                    ln.recordingId = recordingBuffer.noteOn(note, msg.getFloatVelocity(), beatAtSample);

                liveNotes.push_back(ln);
            }
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();

            auto synthNoteOff = MidiMessage::noteOff(1, note, 0.0f);
            filteredMidi.addEvent(synthNoteOff, samplePos);
            midiEngine.noteOff(note, samplePos, outputMidiBuffer);

            auto it = std::find_if(liveNotes.begin(), liveNotes.end(),
                [note, msgChannel, isMPEMemberChannel](const LiveNote& ln)
                {
                    if (ln.noteNumber != note) return false;
                    if (isMPEMemberChannel) return ln.midiChannel == msgChannel;
                    return true;
                });

            if (it != liveNotes.end())
                liveNotes.erase(it);

            if (liveNotes.empty())
            {
                livePitchBendSemitones = 0.0f;
            }

            if (recording)
            {
                double beatAtSample2 = (double)(absoluteSamplePosition + samplePos) * beatsPerSample;
                recordingBuffer.noteOff(note, beatAtSample2);
            }
        }
        else if (msg.isPitchWheel())
        {
            float semitones = ((float)msg.getPitchWheelValue() - 8192.0f) / 8192.0f * (float)midiEngine.getPitchBendRange();

            LiveNote* targetNote = nullptr;

            if (isMPEMemberChannel)
            {
                for (auto& ln : liveNotes)
                {
                    if (ln.midiChannel == msgChannel)
                    {
                        targetNote = &ln;
                        break;
                    }
                }
            }

            if (targetNote == nullptr && !liveNotes.empty())
                targetNote = &liveNotes.back();

            livePitchBendSemitones = semitones;

            if (targetNote != nullptr)
                midiEngine.pitchBend(targetNote->noteNumber, semitones, samplePos, outputMidiBuffer);

            if (recording)
            {
                if (targetNote != nullptr && targetNote->recordingId >= 0)
                {
                    recordingBuffer.pitchBendForNote(targetNote->recordingId, beatAtSample, semitones);
                }
                else
                {
                    recordingBuffer.pitchBend(beatAtSample, semitones);
                }
            }
        }
        else
        {
            filteredMidi.addEvent(msg, samplePos);
            outputMidiBuffer.addEvent(msg, samplePos);
        }
    }

    midiMessages.swapWith(filteredMidi);
}

void Processor::processSequencePlayback(AudioBuffer<float>& buffer,
                                                     MidiBuffer& midiMessages,
                                                     double beatsPerSample, bool hostControlled)
{
    if (!playing)
    {
        return;
    }

    const SpinLock::ScopedTryLockType seqTryLock(sequenceLock);
    if (!seqTryLock.isLocked())
    {
        return;
    }

    double blockStartBeat = playheadBeat;
    const int numSamples = buffer.getNumSamples();
    double blockEndBeat = playheadBeat + numSamples * beatsPerSample;
    needsNoteCatchUp = false;

    bool punchActive = recordingBuffer.isRecording() && !overdubEnabled.load();
    double punchFrom = punchActive ? recordStartBeatInSequence : 1e30;

    auto& allNotes = noteSequence.getAllNotes();

    auto makeKey = [](const NoteData& n) -> std::pair<int,double> {
        return std::make_pair(n.noteNumber, n.startBeat + n.durationBeats * 1e-9);
    };

    std::set<std::pair<int,double>> liveKeysThisBlock;

    for (auto& note : allNotes)
    {
        double eff_start = note.startBeat;
        double eff_end = note.getEndBeat();
        if (punchActive)
        {
            if (eff_start >= punchFrom) continue;
            if (eff_end > punchFrom) eff_end = punchFrom;
        }

        bool overlaps = (eff_end > blockStartBeat) && (eff_start < blockEndBeat);
        if (!overlaps) continue;

        auto key = makeKey(note);
        liveKeysThisBlock.insert(key);

        bool currentlyActive = activeSeqNotes.find(key) != activeSeqNotes.end();

        int onSample  = 0;
        int offSample = numSamples - 1;

        double relStart = eff_start - blockStartBeat;
        if (relStart > 0.0)
            onSample = jlimit(0, numSamples - 1, (int)(relStart / beatsPerSample));

        double relEnd = eff_end - blockStartBeat;
        offSample = jlimit(0, numSamples - 1, (int)(relEnd / beatsPerSample));
        if (offSample <= onSample) offSample = jmin(numSamples - 1, onSample + 1);

        if (!currentlyActive && eff_start >= blockStartBeat)
        {
            int ch = midiEngine.allocFreshChannel(note.noteNumber);
            if (ch > 0)
            {
                midiMessages.addEvent(MidiMessage::noteOn(1, note.noteNumber, note.velocity), onSample);
                midiEngine.noteOnAt(ch, note.noteNumber, note.velocity, onSample, outputMidiBuffer);
                activeSeqNotes[key] = ch;
                currentlyActive = true;
            }
            else
            {
            }
        }
        else if (!currentlyActive && eff_start < blockStartBeat)
        {
            int ch = midiEngine.allocFreshChannel(note.noteNumber);
            if (ch > 0)
            {
                midiMessages.addEvent(MidiMessage::noteOn(1, note.noteNumber, note.velocity), 0);
                midiEngine.noteOnAt(ch, note.noteNumber, note.velocity, 0, outputMidiBuffer);
                activeSeqNotes[key] = ch;
                currentlyActive = true;
            }
            else
            {
            }
        }

        if (currentlyActive && eff_end <= blockEndBeat)
        {
            auto it = activeSeqNotes.find(key);
            if (it != activeSeqNotes.end())
            {
                midiMessages.addEvent(MidiMessage::noteOff(1, note.noteNumber, 0.0f), offSample);
                midiEngine.noteOffAt(it->second, note.noteNumber, offSample, outputMidiBuffer);
                midiEngine.releaseChannel(it->second);
                activeSeqNotes.erase(it);
                liveKeysThisBlock.erase(key);
            }
        }
    }

    for (auto it = activeSeqNotes.begin(); it != activeSeqNotes.end(); )
    {
        if (liveKeysThisBlock.find(it->first) == liveKeysThisBlock.end())
        {
            midiMessages.addEvent(MidiMessage::noteOff(1, it->first.first, 0.0f), 0);
            midiEngine.noteOffAt(it->second, it->first.first, 0, outputMidiBuffer);
            midiEngine.releaseChannel(it->second);
            it = activeSeqNotes.erase(it);
        }
        else
            ++it;
    }

    if (!activeExpression.active)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            double currentBeat = blockStartBeat + i * beatsPerSample;

            if (punchActive && currentBeat >= punchFrom)
                continue;

            bool synthPitchSet = false;

            auto& allSeqNotes = noteSequence.getAllNotes();

            for (size_t noteIdx = 0; noteIdx < allSeqNotes.size(); ++noteIdx)
            {
                auto& note = allSeqNotes[noteIdx];
                if (currentBeat >= note.startBeat && currentBeat < note.getEndBeat())
                {
                    bool  havePitch = false;
                    float pitchAtBeat = (float)note.noteNumber;

                    if (!note.pitchCurve.empty())
                    {
                        pitchAtBeat = note.getPitchAtTime(currentBeat);
                        havePitch = true;
                    }

                    if (havePitch)
                    {
                        float pitchOffset = pitchAtBeat - (float)note.noteNumber;

                        if (!synthPitchSet)
                        {
                            synthPitchSet = true;
                        }

                        if (i % 8 == 0)
                        {
                            auto it = activeSeqNotes.find(std::make_pair(note.noteNumber, note.startBeat + note.durationBeats * 1e-9));
                            if (it != activeSeqNotes.end())
                                midiEngine.pitchBendAt(it->second, pitchOffset, i, outputMidiBuffer);
                        }
                    }
                    else if (!synthPitchSet)
                    {
                        synthPitchSet = true;

                        if (i % 8 == 0)
                        {
                            auto it = activeSeqNotes.find(std::make_pair(note.noteNumber, note.startBeat + note.durationBeats * 1e-9));
                            if (it != activeSeqNotes.end())
                                midiEngine.pitchBendAt(it->second, 0.0f, i, outputMidiBuffer);
                        }
                    }

                    if (!note.amplitudeCurve.empty() && (i % 8 == 0))
                    {
                        auto it = activeSeqNotes.find(std::make_pair(note.noteNumber, note.startBeat + note.durationBeats * 1e-9));
                        if (it != activeSeqNotes.end())
                        {
                            float amp = note.getAmplitudeAtTime(currentBeat);
                            midiEngine.amplitudeAt(it->second, amp, i, outputMidiBuffer);
                        }
                    }
                }
            }

            if (!synthPitchSet && !liveNotes.empty())
            {
            }
        }
    }

    if (!hostControlled)
    {
        playheadBeat = blockEndBeat;
    }
}

AudioProcessorEditor* Processor::createEditor()
{
    return new Editor(*this);
}

PresetManager::State Processor::gatherState() const
{
    PresetManager::State state;

    {
        const SpinLock::ScopedLockType lock(const_cast<SpinLock&>(sequenceLock));
        state.notes = noteSequence.getAllNotes();
    }

    state.bpm = bpm;
    state.masterVolume = masterVolume.load();
    state.midiOutputMode = (midiEngine.getMode() == MidiEngine::Mode::MPE) ? 0 : 1;
    state.midiPitchBendRange = midiEngine.getPitchBendRange();

    return state;
}

void Processor::applyState(const PresetManager::State& state)
{
    playing = false;
    playheadBeat = 0.0;

    requestMidiPanic();
    {
        const SpinLock::ScopedLockType lock(sequenceLock);
        noteSequence.clear();
        for (auto& note : state.notes)
            noteSequence.addNote(note);
    }

    bpm = state.bpm;
    masterVolume.store(state.masterVolume);

    midiEngine.setMode(state.midiOutputMode == 0 ? MidiEngine::Mode::MPE
                                                     : MidiEngine::Mode::Mono);
    midiEngine.setPitchBendRange(state.midiPitchBendRange);

    if (onStateLoaded)
        onStateLoaded();
}

void Processor::getStateInformation(MemoryBlock& destData)
{
    auto state = gatherState();
    PresetManager::stateToBinary(state, destData);
}

void Processor::setStateInformation(const void* data, int sizeInBytes)
{
    ignoreUnused(data, sizeInBytes);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Processor();
}
