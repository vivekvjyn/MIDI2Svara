#pragma once
#include <JuceHeader.h>
#include <vector>

enum class VibratoWaveform
{
    Sine = 0,
    Triangle,
    Humanized,
    Vocal,
    Violin,
    Sitar,
    NumTypes
};

inline const char* vibratoWaveformName(VibratoWaveform w)
{
    switch (w)
    {
        case VibratoWaveform::Sine:      return "Sine";
        case VibratoWaveform::Triangle:  return "Tri";
        case VibratoWaveform::Humanized: return "Human";
        case VibratoWaveform::Vocal:     return "Vocal";
        case VibratoWaveform::Violin:    return "Violin";
        case VibratoWaveform::Sitar:     return "Sitar";
        case VibratoWaveform::NumTypes:
        default:                         return "Sine";
    }
}

struct SegmentVibrato
{
    bool enabled = false;
    float rate = 5.0f;
    float depth = 0.2f;
    float fadeInFrac = 0.15f;
    float fadeOutFrac = 0.15f;
    float offset = 0.0f;
    VibratoWaveform waveform = VibratoWaveform::Sine;
};

struct PitchPoint
{
    double time = 0.0;
    double pitchOffset = 0.0;

    enum class CurveType { Linear, Smooth, Step };
    CurveType curveType = CurveType::Smooth;

    
    
    float curvature = 1.0f;

    
    
    float bias = 0.0f;

    
    SegmentVibrato vibrato;
};

struct AmplitudePoint
{
    double time  = 0.0;
    float  value = 1.0f;
};

struct NoteData
{
    int noteNumber = 60;
    double startBeat = 0.0;
    double durationBeats = 1.0;
    float velocity = 0.8f;

    std::vector<PitchPoint> pitchCurve;
    std::vector<AmplitudePoint> amplitudeCurve;
    bool selected = false;

    double getEndBeat() const { return startBeat + durationBeats; }

    float getPitchAtTime(double beatTime) const;

    
    
    float getAmplitudeAtTime(double beatTime) const;
};

class NoteSequence
{
public:
    void addNote(const NoteData& note) { notes.push_back(note); sortNotes(); }
    void removeNote(int index);
    void clear() { notes.clear(); }

    int getNumNotes() const { return (int)notes.size(); }
    NoteData& getNote(int index) { return notes[(size_t)index]; }
    const NoteData& getNote(int index) const { return notes[(size_t)index]; }

    NoteData* getNoteAt(double beat, int noteNumber, double tolerance = 0.5);
    std::vector<NoteData*> getNotesInRange(double startBeat, double endBeat);

    std::vector<NoteData>& getAllNotes() { return notes; }
    const std::vector<NoteData>& getAllNotes() const { return notes; }
    void sortNotes();
private:

    std::vector<NoteData> notes;
};
