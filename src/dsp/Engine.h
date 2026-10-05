#pragma once
#include <JuceHeader.h>
#include "Shapes.h"
#include "NoteData.h"
#include <map>
#include <random>
#include <set>
#include <vector>

class Engine
{
public:
    Engine();

    void applyExpression(NoteSequence&, bool forceStablePaSa = false, float pitchCorrection = 0.0f);
    bool loadRaga(const String& ragaName);

    void setRootNote(int midiNote) { rootMidiNote = midiNote; }
    int getRootNote() const { return rootMidiNote; }
    String getCurrentRaga() const { return currentRaga; }

    static StringArray getAvailableRagas();
    static const std::map<String, std::set<int>>& getRagaIntervals();
    static int getRagaVadi(const String& ragaName);
    static bool isNoteInRaga(int midiNote, const String& ragaName, int rootNote = 60);

    const std::vector<LookupRow>& getRows() const { return rows; }
    const std::map<String, double>& getPositions() const { return positions; }

private:
    void loadRagaData(const String& ragaName);
    String nearestSvara(double cents) const;
    const LookupRow& selectRow(const String& svara, const String* previous, const String* next);

    String currentRaga;
    int rootMidiNote = 60;

    std::vector<LookupRow> rows;
    std::map<String, size_t> present;
    std::map<String, double> positions;
    std::vector<std::pair<String, double>> byPitch;
    std::vector<int> folded;
    std::mt19937 rng { 0x5eed1234u };
};
