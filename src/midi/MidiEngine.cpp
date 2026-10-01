#include "MidiEngine.h"


MidiEngine::MidiEngine()
{
    for (auto& ch : mpeChannels)
    {
        ch.noteNumber = -1;
        ch.active = false;
    }
}

void MidiEngine::setMode(Mode m)
{
    mode = m;
    if (mode == Mode::MPE)
        pitchBendRangeSemitones = 48;
    else
        pitchBendRangeSemitones = 2;
}

void MidiEngine::setPitchBendRange(int semitones)
{
    pitchBendRangeSemitones = jlimit(1, 96, semitones);
}

void MidiEngine::prepareBlock(int )
{
    
}

int MidiEngine::allocateMPEChannel(int noteNumber)
{
    
    for (int i = 0; i < numMPEChannels; ++i)
    {
        if (!mpeChannels[(size_t)i].explicitAlloc
            && mpeChannels[(size_t)i].noteNumber == noteNumber
            && mpeChannels[(size_t)i].active)
            return i + 2;
    }

    for (int attempt = 0; attempt < numMPEChannels; ++attempt)
    {
        int idx = (nextRoundRobin + attempt) % numMPEChannels;
        if (!mpeChannels[(size_t)idx].active)
        {
            mpeChannels[(size_t)idx].noteNumber = noteNumber;
            mpeChannels[(size_t)idx].active = true;
            mpeChannels[(size_t)idx].explicitAlloc = false;
            nextRoundRobin = (idx + 1) % numMPEChannels;
            return idx + 2;
        }
    }

    for (int attempt = 0; attempt < numMPEChannels; ++attempt)
    {
        int idx = (nextRoundRobin + attempt) % numMPEChannels;
        if (!mpeChannels[(size_t)idx].explicitAlloc)
        {
            mpeChannels[(size_t)idx].noteNumber = noteNumber;
            mpeChannels[(size_t)idx].active = true;
            mpeChannels[(size_t)idx].explicitAlloc = false;
            nextRoundRobin = (idx + 1) % numMPEChannels;
            return idx + 2;
        }
    }
    
    int idx = nextRoundRobin;
    mpeChannels[(size_t)idx].noteNumber = noteNumber;
    mpeChannels[(size_t)idx].active = true;
    mpeChannels[(size_t)idx].explicitAlloc = false;
    nextRoundRobin = (idx + 1) % numMPEChannels;
    return idx + 2;
}

void MidiEngine::freeMPEChannel(int noteNumber)
{
    for (auto& ch : mpeChannels)
    {
        if (!ch.explicitAlloc && ch.noteNumber == noteNumber && ch.active)
        {
            ch.active = false;
            ch.noteNumber = -1;
            return;
        }
    }
}

int MidiEngine::getChannelForNote(int noteNumber) const
{
    if (mode == Mode::Mono)
        return 1;

    for (int i = 0; i < numMPEChannels; ++i)
    {
        if (!mpeChannels[(size_t)i].explicitAlloc
            && mpeChannels[(size_t)i].noteNumber == noteNumber
            && mpeChannels[(size_t)i].active)
            return i + 2;
    }
    return -1; 
}

int MidiEngine::semitonesToPitchWheel(float semitones) const
{
    if (pitchBendRangeSemitones <= 0) return 8192;
    float normalized = semitones / (float)pitchBendRangeSemitones;
    int value = roundToInt(8192.0f + normalized * 8191.0f);
    return jlimit(0, 16383, value);
}

void MidiEngine::noteOn(int noteNumber, float velocity, int sampleOffset,
                                  MidiBuffer& output)
{
    if (mode == Mode::MPE)
    {
        int channel = allocateMPEChannel(noteNumber);

        output.addEvent(MidiMessage::pitchWheel(channel, 8192), sampleOffset);
        output.addEvent(MidiMessage::noteOn(channel, noteNumber, velocity), sampleOffset);
    }
    else
    {
        
        monoLastNote = noteNumber;
        output.addEvent(MidiMessage::noteOn(1, noteNumber, velocity), sampleOffset);
    }
}

void MidiEngine::noteOff(int noteNumber, int sampleOffset, MidiBuffer& output)
{
    if (mode == Mode::MPE)
    {
        int channel = getChannelForNote(noteNumber);
        if (channel < 0) channel = 1; 

        output.addEvent(MidiMessage::noteOff(channel, noteNumber, 0.0f), sampleOffset);
        freeMPEChannel(noteNumber);
    }
    else
    {
        output.addEvent(MidiMessage::noteOff(1, noteNumber, 0.0f), sampleOffset);
        if (monoLastNote == noteNumber)
            monoLastNote = -1;
    }
}

void MidiEngine::pitchBend(int noteNumber, float pitchOffsetSemitones,
                                     int sampleOffset, MidiBuffer& output)
{
    int wheelValue = semitonesToPitchWheel(pitchOffsetSemitones);

    if (mode == Mode::MPE)
    {
        int channel = getChannelForNote(noteNumber);
        if (channel < 0) return; 

        output.addEvent(MidiMessage::pitchWheel(channel, wheelValue), sampleOffset);
    }
    else
    {
        
        if (noteNumber == monoLastNote || monoLastNote < 0)
            output.addEvent(MidiMessage::pitchWheel(1, wheelValue), sampleOffset);
    }
}

void MidiEngine::sendMPEConfiguration(MidiBuffer& output)
{
    
    int channel = 1;

    output.addEvent(MidiMessage::controllerEvent(channel, 101, 0), 0);
    
    output.addEvent(MidiMessage::controllerEvent(channel, 100, 6), 0);
    
    output.addEvent(MidiMessage::controllerEvent(channel, 6, numMPEChannels), 0);
    
    output.addEvent(MidiMessage::controllerEvent(channel, 38, 0), 0);

    for (int ch = 2; ch <= 16; ++ch)
    {
        output.addEvent(MidiMessage::controllerEvent(ch, 101, 0), 0);
        output.addEvent(MidiMessage::controllerEvent(ch, 100, 0), 0);
        output.addEvent(MidiMessage::controllerEvent(ch, 6, pitchBendRangeSemitones), 0);
        output.addEvent(MidiMessage::controllerEvent(ch, 38, 0), 0);
    }
}

void MidiEngine::allNotesOff(MidiBuffer& output, int sampleOffset)
{
    if (mode == Mode::MPE)
    {
        for (int i = 0; i < numMPEChannels; ++i)
        {
            if (mpeChannels[(size_t)i].active)
            {
                int channel = i + 2;
                output.addEvent(
                    MidiMessage::noteOff(channel, mpeChannels[(size_t)i].noteNumber, 0.0f),
                    sampleOffset);
                mpeChannels[(size_t)i].active = false;
                mpeChannels[(size_t)i].noteNumber = -1;
                mpeChannels[(size_t)i].explicitAlloc = false;
            }
        }
    }
    else
    {
        
        output.addEvent(MidiMessage::allNotesOff(1), sampleOffset);
        monoLastNote = -1;
    }

    nextRoundRobin = 0;
}

int MidiEngine::allocFreshChannel(int noteNumber)
{
    if (mode == Mode::Mono) return 1;

    for (int attempt = 0; attempt < numMPEChannels; ++attempt)
    {
        int idx = (nextRoundRobin + attempt) % numMPEChannels;
        if (!mpeChannels[(size_t)idx].active)
        {
            mpeChannels[(size_t)idx].active = true;
            mpeChannels[(size_t)idx].noteNumber = noteNumber;
            mpeChannels[(size_t)idx].explicitAlloc = true;
            nextRoundRobin = (idx + 1) % numMPEChannels;
            return idx + 2;
        }
    }

    return -1;
}

void MidiEngine::releaseChannel(int channel)
{
    if (mode == Mode::Mono) return;
    int idx = channel - 2;
    if (idx < 0 || idx >= numMPEChannels) return;
    mpeChannels[(size_t)idx].active = false;
    mpeChannels[(size_t)idx].noteNumber = -1;
    mpeChannels[(size_t)idx].explicitAlloc = false;
}

void MidiEngine::noteOnAt(int channel, int noteNumber, float velocity,
                                    int sampleOffset, MidiBuffer& output)
{
    if (mode == Mode::MPE)
    {
        output.addEvent(MidiMessage::pitchWheel(channel, 8192), sampleOffset);
        output.addEvent(MidiMessage::noteOn(channel, noteNumber, velocity), sampleOffset);
    }
    else
    {
        monoLastNote = noteNumber;
        output.addEvent(MidiMessage::noteOn(1, noteNumber, velocity), sampleOffset);
    }
}

void MidiEngine::noteOffAt(int channel, int noteNumber, int sampleOffset,
                                     MidiBuffer& output)
{
    if (mode == Mode::MPE)
    {
        output.addEvent(MidiMessage::noteOff(channel, noteNumber, 0.0f), sampleOffset);
    }
    else
    {
        output.addEvent(MidiMessage::noteOff(1, noteNumber, 0.0f), sampleOffset);
        if (monoLastNote == noteNumber) monoLastNote = -1;
    }
}

void MidiEngine::pitchBendAt(int channel, float pitchOffsetSemitones,
                                       int sampleOffset, MidiBuffer& output)
{
    int wheelValue = semitonesToPitchWheel(pitchOffsetSemitones);
    if (mode == Mode::MPE)
        output.addEvent(MidiMessage::pitchWheel(channel, wheelValue), sampleOffset);
    else
        output.addEvent(MidiMessage::pitchWheel(1, wheelValue), sampleOffset);
}

void MidiEngine::setAmplitudeMode(AmplitudeMode m)
{
    if (m == amplitudeMode) return;
    amplitudeMode = m;
    
    for (auto& v : lastAmplitudeValue) v = -1;
}

void MidiEngine::setCustomAmplitudeCC(int ccNumber)
{
    customAmplitudeCC = jlimit(0, 127, ccNumber);
    for (auto& v : lastAmplitudeValue) v = -1;
}

void MidiEngine::amplitudeAt(int channel, float value0to1,
                                       int sampleOffset, MidiBuffer& output)
{
    if (amplitudeMode == AmplitudeMode::None) return;
    if (channel < 1 || channel > 16) return;

    int outChannel = (mode == Mode::MPE) ? channel : 1;

    int v7 = jlimit(0, 127, (int)std::round(jlimit(0.0f, 1.0f, value0to1) * 127.0f));

    if (lastAmplitudeValue[(size_t)outChannel] == v7) return;
    lastAmplitudeValue[(size_t)outChannel] = v7;

    MidiMessage msg;
    switch (amplitudeMode)
    {
        case AmplitudeMode::Pressure:
            msg = MidiMessage::channelPressureChange(outChannel, v7);
            break;
        case AmplitudeMode::ModWheel:
            msg = MidiMessage::controllerEvent(outChannel, 1, v7);
            break;
        case AmplitudeMode::Breath:
            msg = MidiMessage::controllerEvent(outChannel, 2, v7);
            break;
        case AmplitudeMode::Volume:
            msg = MidiMessage::controllerEvent(outChannel, 7, v7);
            break;
        case AmplitudeMode::Expression:
            msg = MidiMessage::controllerEvent(outChannel, 11, v7);
            break;
        case AmplitudeMode::CustomCC:
            msg = MidiMessage::controllerEvent(outChannel, customAmplitudeCC, v7);
            break;
        case AmplitudeMode::None:
            return;
    }
    output.addEvent(msg, sampleOffset);
}
