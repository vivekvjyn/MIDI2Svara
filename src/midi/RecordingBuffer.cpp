#include "RecordingBuffer.h"
#include "../dsp/PitchCurve.h"
#include <algorithm>
#include <cmath>

void RecordingBuffer::startRecording(double startBeat)
{
    clear();
    recordStartBeat = startBeat;
    recording.store(true);
}

void RecordingBuffer::stopRecording(double endBeat)
{
    recording.store(false);

    for (auto& evt : noteEvents)
    {
        if (evt.beatOff < 0.0)
        {
            evt.beatOff = (endBeat > evt.beatOn) ? endBeat : (evt.beatOn + 1.0);
        }
    }
}

int RecordingBuffer::noteOn(int noteNumber, float velocity, double beat)
{
    RecordedNoteEvent evt;
    evt.id = nextNoteId++;
    evt.noteNumber = noteNumber;
    evt.velocity = velocity;
    evt.beatOn = beat;
    noteEvents.push_back(evt);
    return evt.id;
}

void RecordingBuffer::noteOff(int noteNumber, double beat)
{
    
    for (int i = (int)noteEvents.size() - 1; i >= 0; --i)
    {
        if (noteEvents[(size_t)i].noteNumber == noteNumber && noteEvents[(size_t)i].beatOff < 0.0)
        {
            noteEvents[(size_t)i].beatOff = beat;
            return;
        }
    }
}

int RecordingBuffer::getActiveNoteId(int noteNumber) const
{
    
    for (int i = (int)noteEvents.size() - 1; i >= 0; --i)
    {
        if (noteEvents[(size_t)i].noteNumber == noteNumber && noteEvents[(size_t)i].beatOff < 0.0)
            return noteEvents[(size_t)i].id;
    }
    return -1;
}

void RecordingBuffer::pitchBend(double beat, float semitones)
{
    pitchBends.push_back({ beat, semitones });
}

void RecordingBuffer::pitchBendForNote(int noteId, double beat, float semitones)
{
    perNotePitchBends[noteId].push_back({ beat, semitones });
}

void RecordingBuffer::expressionOffset(double beat, float semitones)
{
    expressionOffsets.push_back({ beat, semitones });
}

static float interpolateSamples(const std::vector<RecordedPitchSample>& samples,
                                double beat)
{
    if (samples.empty()) return 0.0f;
    if (beat <= samples.front().beat) return samples.front().semitones;
    if (beat >= samples.back().beat) return samples.back().semitones;

    for (size_t i = 1; i < samples.size(); ++i)
    {
        if (samples[i].beat >= beat)
        {
            double t = (beat - samples[i - 1].beat) / (samples[i].beat - samples[i - 1].beat);
            return samples[i - 1].semitones + (float)t * (samples[i].semitones - samples[i - 1].semitones);
        }
    }
    return samples.back().semitones;
}

std::vector<NoteData> RecordingBuffer::toNoteData() const
{
    std::vector<NoteData> result;

    auto collectSamples = [](const std::vector<RecordedPitchSample>& src,
                             double noteOn, double noteOff) -> std::vector<RecordedPitchSample>
    {
        std::vector<RecordedPitchSample> out;
        for (auto& s : src)
        {
            if (s.beat >= noteOn && s.beat <= noteOff)
                out.push_back(s);
        }
        return out;
    };

    for (auto& evt : noteEvents)
    {
        NoteData nd;
        nd.noteNumber = evt.noteNumber;
        nd.velocity = evt.velocity;
        nd.startBeat = evt.beatOn - recordStartBeat;
        nd.durationBeats = (evt.beatOff >= 0.0)
                               ? (evt.beatOff - evt.beatOn)
                               : 1.0;

        if (nd.durationBeats < 0.1)
            nd.durationBeats = 0.1;

        std::vector<RecordedPitchSample> pbSamples;
        auto perNoteIt = perNotePitchBends.find(evt.id);

        if (perNoteIt != perNotePitchBends.end() && !perNoteIt->second.empty())
            pbSamples = collectSamples(perNoteIt->second, evt.beatOn, evt.beatOff);
        else
            pbSamples = collectSamples(pitchBends, evt.beatOn, evt.beatOff);

        auto exSamples = collectSamples(expressionOffsets, evt.beatOn, evt.beatOff);

        bool hasPB = !pbSamples.empty();
        bool hasExpr = !exSamples.empty();

        if (hasPB || hasExpr)
        {
            
            std::vector<double> timestamps;
            timestamps.push_back(evt.beatOn);

            for (auto& s : pbSamples) timestamps.push_back(s.beat);
            for (auto& s : exSamples) timestamps.push_back(s.beat);

            std::sort(timestamps.begin(), timestamps.end());
            timestamps.erase(std::unique(timestamps.begin(), timestamps.end(),
                [](double a, double b) { return std::abs(a - b) < 0.0001; }),
                timestamps.end());

            if (timestamps.size() > 200)
            {
                std::vector<double> reduced;
                reduced.push_back(timestamps.front());
                double step = (timestamps.back() - timestamps.front()) / 199.0;
                for (int i = 1; i < 199; ++i)
                    reduced.push_back(timestamps.front() + i * step);
                reduced.push_back(timestamps.back());
                timestamps = reduced;
            }

            for (auto beat : timestamps)
            {
                float pbVal = hasPB ? interpolateSamples(pbSamples, beat) : 0.0f;
                float exVal = hasExpr ? interpolateSamples(exSamples, beat) : 0.0f;
                float total = pbVal + exVal;

                PitchPoint pp;
                pp.time = beat - evt.beatOn;
                pp.pitchOffset = (double)total;
                pp.curveType = PitchPoint::CurveType::Smooth;
                nd.pitchCurve.push_back(pp);
            }

            if (nd.pitchCurve.front().time > 0.001)
            {
                PitchPoint startPt;
                startPt.time = 0.0;
                startPt.pitchOffset = 0.0;
                startPt.curveType = PitchPoint::CurveType::Linear;
                nd.pitchCurve.insert(nd.pitchCurve.begin(), startPt);
            }

            if (nd.pitchCurve.back().time < nd.durationBeats - 0.001)
            {
                PitchPoint endPt;
                endPt.time = nd.durationBeats;
                endPt.pitchOffset = nd.pitchCurve.back().pitchOffset;
                endPt.curveType = PitchPoint::CurveType::Linear;
                nd.pitchCurve.push_back(endPt);
            }
        }

        result.push_back(nd);
    }

    return result;
}

void RecordingBuffer::clear()
{
    noteEvents.clear();
    pitchBends.clear();
    perNotePitchBends.clear();
    expressionOffsets.clear();
    recordStartBeat = 0.0;
    nextNoteId = 1;
}
