#pragma once
#include <JuceHeader.h>
#include <array>

class MidiEngine
{
public:
    enum class Mode { MPE, Mono };

    
    enum class AmplitudeMode
    {
        None       = 0,   
        Pressure   = 1,   
        ModWheel   = 2,   
        Breath     = 3,   
        Volume     = 4,   
        Expression = 5,   
        CustomCC   = 6    
    };

    MidiEngine();

    void setMode(Mode m);
    Mode getMode() const { return mode; }

    void setPitchBendRange(int semitones);
    int getPitchBendRange() const { return pitchBendRangeSemitones; }

    
    void prepareBlock(int numSamples);

    
    void noteOn(int noteNumber, float velocity, int sampleOffset, MidiBuffer& output);

    
    void noteOff(int noteNumber, int sampleOffset, MidiBuffer& output);

    
    
    void pitchBend(int noteNumber, float pitchOffsetSemitones, int sampleOffset, MidiBuffer& output);

    
    void sendMPEConfiguration(MidiBuffer& output);

    
    void allNotesOff(MidiBuffer& output, int sampleOffset);

    
    
    
    
    int  allocFreshChannel(int noteNumber);
    void releaseChannel(int channel);
    void noteOnAt(int channel, int noteNumber, float velocity, int sampleOffset, MidiBuffer& output);
    void noteOffAt(int channel, int noteNumber, int sampleOffset, MidiBuffer& output);
    void pitchBendAt(int channel, float pitchOffsetSemitones, int sampleOffset, MidiBuffer& output);

    
    void  setAmplitudeMode(AmplitudeMode m);
    AmplitudeMode getAmplitudeMode() const { return amplitudeMode; }

    
    void  setCustomAmplitudeCC(int ccNumber);
    int   getCustomAmplitudeCC() const { return customAmplitudeCC; }

    
    
    
    
    void amplitudeAt(int channel, float value0to1, int sampleOffset, MidiBuffer& output);

private:
    Mode mode = Mode::MPE;
    int pitchBendRangeSemitones = 48; 

    AmplitudeMode amplitudeMode = AmplitudeMode::Expression;
    int           customAmplitudeCC = 11; 
    
    
    std::array<int, 17> lastAmplitudeValue { -1, -1, -1, -1, -1, -1, -1, -1,
                                             -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    
    
    static constexpr int numMPEChannels = 15;

    struct ChannelSlot {
        int noteNumber = -1;  
        bool active = false;
        bool explicitAlloc = false; 
    };
    std::array<ChannelSlot, numMPEChannels> mpeChannels;
    int nextRoundRobin = 0;

    int allocateMPEChannel(int noteNumber);
    void freeMPEChannel(int noteNumber);
    int getChannelForNote(int noteNumber) const;

    
    int monoLastNote = -1;

    
    int semitonesToPitchWheel(float semitones) const;
};
