#pragma once
#include <JuceHeader.h>
#include "NoteData.h"
#include <limits>
#include <map>
#include <set>
#include <vector>

/** One annotated svara: the melodic context it was sung in, and the contour that
    was sung. Pitch is in cents relative to the svara's own position, so a sample
    transposes to any tonic or octave unchanged.
*/
struct GamakaSample
{
    static constexpr int unknownInterval = std::numeric_limits<int>::min();

    int svarasthana = 0;                      // pitch position in cents, folded into one octave
    int previousInterval = unknownInterval;  // cents from this svara to the previous one
    int nextInterval = unknownInterval;  // cents from this svara to the next one
    float durationBeats = 1.0f;
    float excursionCents = 0.0f;                  // peak-to-peak excursion, cents

    std::vector<float> normalizedTime;                // normalized time, 0..1
    std::vector<float> pitchOffsetCents;                // cents relative to this svara
};

struct GamakaQuery
{
    int svarasthana = 0;
    int previousInterval = GamakaSample::unknownInterval;
    int nextInterval = GamakaSample::unknownInterval;
    float durationBeats = 1.0f;
    float excursionCents = 0.0f;
};

class CarnaticEngine
{
public:
    CarnaticEngine();

    void applyExpression(NoteSequence&, bool forceStablePaSa = false, float pitchCorrection = 0.0f);
    bool loadRaga(const String& ragaName);

    void setRootNote(int midiNote) { rootMidiNote = midiNote; }
    int getRootNote() const { return rootMidiNote; }
    String getCurrentRaga() const { return currentRaga; }
    int getNumSamples() const { return (int)samples.size(); }

    static StringArray getAvailableRagas();
    static const std::map<String, std::set<int>>& getRagaIntervals();
    static bool isNoteInRaga(int midiNote, const String& ragaName, int rootNote = 60);

    /** Distance-weighted blend of the nearest matching samples, on a uniform grid
        spanning the note. Empty only when the raga has no usable data at all.
    */
    std::vector<float> synthesize(const GamakaQuery&) const;

private:
    void loadRagaData(const String& ragaName);

    float distance(const GamakaSample&, const GamakaQuery&) const;
    int nearestSvarasthana(int svarasthana) const;

    String currentRaga;
    int rootMidiNote = 60;

    std::vector<GamakaSample> samples;
    std::vector<int> scale;
};
