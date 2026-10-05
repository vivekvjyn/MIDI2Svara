#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"
#include <vector>
#include <map>
#include <atomic>

struct RecordedNoteEvent
{
    int id = 0;
    int noteNumber = 60;
    float velocity = 0.8f;
    double beatOn = 0.0;
    double beatOff = -1.0;
};

struct RecordedPitchSample
{
    double beat = 0.0;
    float semitones = 0.0f;
};

class RecordingBuffer
{
public:
    void startRecording(double startBeat);
    void stopRecording(double endBeat);
    bool isRecording() const { return recording.load(); }

    
    int noteOn(int noteNumber, float velocity, double beat);
    void noteOff(int noteNumber, double beat);
    void pitchBend(double beat, float semitones);
    void pitchBendForNote(int noteId, double beat, float semitones);
    void expressionOffset(double beat, float semitones);

    
    int getActiveNoteId(int noteNumber) const;

    
    std::vector<NoteData> toNoteData() const;

    
    const std::vector<RecordedNoteEvent>& getNoteEvents() const { return noteEvents; }
    const std::vector<RecordedPitchSample>& getPitchBends() const { return pitchBends; }
    const std::map<int, std::vector<RecordedPitchSample>>& getPerNotePitchBends() const { return perNotePitchBends; }
    const std::vector<RecordedPitchSample>& getExpressionOffsets() const { return expressionOffsets; }
    double getRecordStartBeat() const { return recordStartBeat; }

    void clear();

private:
    std::atomic<bool> recording { false };
    double recordStartBeat = 0.0;
    int nextNoteId = 1;
    std::vector<RecordedNoteEvent> noteEvents;
    std::vector<RecordedPitchSample> pitchBends;
    std::map<int, std::vector<RecordedPitchSample>> perNotePitchBends;
    std::vector<RecordedPitchSample> expressionOffsets;
};
