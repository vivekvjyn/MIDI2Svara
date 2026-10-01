#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"
#include "../midi/MidiEngine.h"

class PresetManager
{
public:
    struct State
    {
        std::vector<NoteData> notes;
        double bpm = 120.0;
        float masterVolume = 0.8f;
        int midiOutputMode = 0;
        int midiPitchBendRange = 48;
        double viewStartBeat = 0.0;
        double viewEndBeat = 16.0;
        double viewLowest = 36.0;
        double viewHighest = 84.0;
    };

    static XmlElement stateToXml(const State& state);
    static State xmlToState(const XmlElement& xml);
    static void stateToBinary(const State& state, MemoryBlock& destData);
    static State binaryToState(const void* data, int sizeInBytes);
    static bool saveToFile(const State& state, const File& file);
    static State loadFromFile(const File& file, bool& success);
    static File getPresetDirectory();
    static constexpr const char* fileExtension = ".fp2";

private:
    static XmlElement noteToXml(const NoteData& note);
    static NoteData xmlToNote(const XmlElement& xml);
    static XmlElement pitchPointToXml(const PitchPoint& pt);
    static PitchPoint xmlToPitchPoint(const XmlElement& xml);
    static XmlElement vibratoToXml(const SegmentVibrato& vib);
    static SegmentVibrato xmlToVibrato(const XmlElement& xml);
};
