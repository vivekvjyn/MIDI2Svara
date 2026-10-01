#include "CarnaticEngine.h"
#include <BinaryData.h>
#include <rapidjson/document.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
    // Matching weights, all reduced to a common "cents of melodic difference" scale.
    // Svarasthana is weighted so heavily that a sample for a different svara can never
    // outrank one for the right svara - it is the svara's identity, not a nudge.
    constexpr float svarasthanaWeight = 4.0f;
    constexpr float contextWeight     = 1.0f;
    constexpr float durationWeight    = 120.0f;
    constexpr float excursionWeight   = 0.15f;

    // Cost of matching against a neighbour we do not know. High enough to prefer a
    // known-context sample, low enough that phrase-edge notes still find something.
    constexpr float unknownPenalty  = 250.0f;

    constexpr int   maxNeighbours   = 5;
    constexpr float neighbourWindow = 300.0f;  // discard matches this much worse than the best
    constexpr int   gridSize        = 64;
    constexpr float simplifyCents   = 4.0f;

    int floorMod(int value, int modulus)
    {
        int result = value % modulus;
        return (result < 0) ? result + modulus : result;
    }

    /** Shortest signed distance between two positions on the octave circle. */
    int shortestCentsBetween(int a, int b)
    {
        int diff = floorMod(a - b, 1200);
        return (diff > 600) ? diff - 1200 : diff;
    }

    float interpolateAt(const std::vector<float>& x, const std::vector<float>& y, float t)
    {
        if (x.empty())
            return 0.0f;
        if (t <= x.front())
            return y.front();
        if (t >= x.back())
            return y.back();

        auto upper = std::upper_bound(x.begin(), x.end(), t);
        size_t i = (size_t)std::distance(x.begin(), upper) - 1;
        if (i + 1 >= x.size())
            return y.back();

        float span = x[i + 1] - x[i];
        if (span <= 1.0e-9f)
            return y[i + 1];

        return y[i] + (y[i + 1] - y[i]) * (t - x[i]) / span;
    }

    /** Ramer-Douglas-Peucker over a curve already sampled on a uniform grid. */
    std::vector<int> douglasPeuckerIndices(const std::vector<float>& values, float tolerance)
    {
        const int n = (int)values.size();
        std::vector<bool> keep((size_t)n, false);
        keep.front() = keep.back() = true;

        std::vector<std::pair<int, int>> stack{ { 0, n - 1 } };
        const float xScale = 200.0f / (float)(n - 1);

        while (!stack.empty())
        {
            auto [start, end] = stack.back();
            stack.pop_back();
            if (end <= start + 1)
                continue;

            float dx = (float)(end - start) * xScale;
            float dy = values[(size_t)end] - values[(size_t)start];
            float norm = std::sqrt(dx * dx + dy * dy);

            int worst = -1;
            float worstDistance = tolerance;

            for (int i = start + 1; i < end; ++i)
            {
                float px = (float)(i - start) * xScale;
                float py = values[(size_t)i] - values[(size_t)start];
                float distance = (norm < 1.0e-9f)
                    ? std::sqrt(px * px + py * py)
                    : std::abs(dy * px - dx * py) / norm;

                if (distance > worstDistance)
                {
                    worstDistance = distance;
                    worst = i;
                }
            }

            if (worst >= 0)
            {
                keep[(size_t)worst] = true;
                stack.push_back({ start, worst });
                stack.push_back({ worst, end });
            }
        }

        std::vector<int> indices;
        for (int i = 0; i < n; ++i)
            if (keep[(size_t)i])
                indices.push_back(i);
        return indices;
    }

    const char* findRagaResource(const String& raga, int& outSize)
    {
        struct Entry { const char* name; const char* data; int size; };

    #define FP2_RAGA_ENTRY(R) { #R, reinterpret_cast<const char*>(RagaData::R##_json), RagaData::R##_jsonSize }

        static const Entry entries[] = {
            FP2_RAGA_ENTRY(abhogi), FP2_RAGA_ENTRY(begada), FP2_RAGA_ENTRY(kalyani), FP2_RAGA_ENTRY(mohanam),
            FP2_RAGA_ENTRY(sahana), FP2_RAGA_ENTRY(saveri),  FP2_RAGA_ENTRY(sri),
        };

    #undef FP2_RAGA_ENTRY

        for (const auto& entry : entries)
        {
            if (std::strcmp(entry.name, raga.toRawUTF8()) == 0)
            {
                outSize = entry.size;
                return entry.data;
            }
        }

        outSize = 0;
        return nullptr;
    }
}

CarnaticEngine::CarnaticEngine()
{
    auto ragas = getAvailableRagas();
    if (!ragas.isEmpty())
        loadRaga(ragas[0]);
}

StringArray CarnaticEngine::getAvailableRagas()
{
    return { "abhogi", "begada", "kalyani", "mohanam", "sahana", "saveri", "sri" };
}

const std::map<String, std::set<int>>& CarnaticEngine::getRagaIntervals()
{
    static const std::map<String, std::set<int>> intervals = {
        { "abhogi",  {0, 2, 3, 5, 9} },
        { "begada",  {0, 2, 4, 5, 7, 9} },
        { "kalyani", {0, 2, 4, 6, 7, 9, 11} },
        { "mohanam", {0, 2, 4, 7, 9} },
        { "sahana",  {0, 2, 3, 5, 7, 9, 11} },
        { "saveri",  {0, 1, 5, 7, 8} },
        { "sri",     {0, 1, 4, 5, 7, 8, 10} },
    };
    return intervals;
}

bool CarnaticEngine::isNoteInRaga(int midiNote, const String& ragaName, int rootNote)
{
    const auto& intervals = getRagaIntervals();
    auto it = intervals.find(ragaName);
    if (it == intervals.end())
        return true;
    return it->second.count(floorMod(midiNote - rootNote, 12)) > 0;
}

bool CarnaticEngine::loadRaga(const String& ragaName)
{
    loadRagaData(ragaName);
    currentRaga = ragaName;
    return !samples.empty();
}

void CarnaticEngine::loadRagaData(const String& ragaName)
{
    samples.clear();
    scale.clear();

    int size = 0;
    const char* data = findRagaResource(ragaName, size);
    if (data == nullptr || size <= 0)
        return;

    rapidjson::Document doc;
    doc.Parse(data, (size_t)size);
    if (doc.HasParseError() || !doc.IsObject())
        return;

    if (doc.HasMember("scale") && doc["scale"].IsArray())
        for (const auto& svarasthana : doc["scale"].GetArray())
            scale.push_back(svarasthana.GetInt());

    if (!doc.HasMember("samples") || !doc["samples"].IsArray())
        return;

    const auto& array = doc["samples"];
    samples.reserve(array.Size());

    for (const auto& entry : array.GetArray())
    {
        if (!entry.IsObject() || !entry.HasMember("normalizedTime") || !entry.HasMember("pitchOffsetCents"))
            continue;

        GamakaSample sample;
        sample.svarasthana = entry["svarasthana"].GetInt();
        sample.previousInterval = entry["previousInterval"].IsNull()
                                ? GamakaSample::unknownInterval : entry["previousInterval"].GetInt();
        sample.nextInterval = entry["nextInterval"].IsNull()
                            ? GamakaSample::unknownInterval : entry["nextInterval"].GetInt();
        sample.durationBeats = entry["durationBeats"].GetFloat();
        sample.excursionCents = entry["excursionCents"].GetFloat();

        for (const auto& value : entry["normalizedTime"].GetArray())
            sample.normalizedTime.push_back(value.GetFloat());
        for (const auto& value : entry["pitchOffsetCents"].GetArray())
            sample.pitchOffsetCents.push_back(value.GetFloat());

        if (sample.normalizedTime.size() >= 2
            && sample.normalizedTime.size() == sample.pitchOffsetCents.size())
            samples.push_back(std::move(sample));
    }
}

int CarnaticEngine::nearestSvarasthana(int svarasthana) const
{
    if (scale.empty())
        return svarasthana;

    int best = scale.front();
    int bestDistance = std::numeric_limits<int>::max();

    for (int candidate : scale)
    {
        int distance = std::abs(shortestCentsBetween(candidate, svarasthana));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

float CarnaticEngine::distance(const GamakaSample& sample, const GamakaQuery& query) const
{
    float total = svarasthanaWeight * (float)std::abs(shortestCentsBetween(sample.svarasthana, query.svarasthana));

    auto contextCost = [](int a, int b)
    {
        if (a == GamakaSample::unknownInterval || b == GamakaSample::unknownInterval)
            return unknownPenalty;
        return contextWeight * (float)std::abs(a - b);
    };

    total += contextCost(sample.previousInterval, query.previousInterval);
    total += contextCost(sample.nextInterval, query.nextInterval);

    float beats = std::max(sample.durationBeats, 1.0e-3f);
    float target = std::max(query.durationBeats, 1.0e-3f);
    total += durationWeight * std::abs(std::log2(beats / target));

    total += excursionWeight * std::abs(sample.excursionCents - query.excursionCents);

    return total;
}

std::vector<float> CarnaticEngine::synthesize(const GamakaQuery& query) const
{
    if (samples.empty())
        return {};

    struct Match { float distance; const GamakaSample* sample; };
    std::vector<Match> matches;
    matches.reserve(samples.size());

    for (const auto& sample : samples)
        matches.push_back({ distance(sample, query), &sample });

    const size_t count = std::min((size_t)maxNeighbours, matches.size());
    std::partial_sort(matches.begin(), matches.begin() + (long)count, matches.end(),
                      [](const Match& a, const Match& b) { return a.distance < b.distance; });

    // Blending in a poor match muddies a good one, so only average over samples
    // that are genuinely comparable to the best.
    const float cutoff = matches.front().distance + neighbourWindow;

    std::vector<float> grid((size_t)gridSize, 0.0f);
    float totalWeight = 0.0f;

    for (size_t i = 0; i < count; ++i)
    {
        if (matches[i].distance > cutoff)
            break;

        // Every candidate is resampled onto the shared grid before averaging.
        // Averaging change points by index would mix, say, the third point of a
        // 6-point curve with the third of a 14-point curve, which means nothing.
        const float weight = 1.0f / (matches[i].distance + 25.0f);
        const auto& sample = *matches[i].sample;

        for (int k = 0; k < gridSize; ++k)
            grid[(size_t)k] += weight * interpolateAt(sample.normalizedTime, sample.pitchOffsetCents, (float)k / (float)(gridSize - 1));

        totalWeight += weight;
    }

    if (totalWeight <= 0.0f)
        return {};

    for (auto& value : grid)
        value /= totalWeight;

    return grid;
}

void CarnaticEngine::applyExpression(NoteSequence& sequence, bool forceStablePaSa, float pitchCorrection)
{
    auto& notes = sequence.getAllNotes();
    if (notes.empty())
        return;

    std::sort(notes.begin(), notes.end(),
              [](const NoteData& a, const NoteData& b) { return a.startBeat < b.startBeat; });

    // Velocity drives how strongly the note is ornamented, by asking the matcher for
    // a gamaka of a given excursion rather than by scaling the result afterwards.
    constexpr float quietRange = 150.0f;
    constexpr float loudRange = 700.0f;

    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& note = notes[i];
        note.pitchCurve.clear();

        const int currentCents = (note.noteNumber - rootMidiNote) * 100;
        const int svarasthana = floorMod(currentCents, 1200);
        const int matched = nearestSvarasthana(svarasthana);

        // Off-scale notes are pulled toward the raga's own svarasthana, and that offset
        // rides on top of whatever contour is applied.
        const float correction = pitchCorrection * (float)shortestCentsBetween(matched, svarasthana);

        auto applyFlat = [&note](float offsetCents)
        {
            if (std::abs(offsetCents) > 1.0e-3f)
            {
                PitchPoint point;
                point.time = 0.0;
                point.pitchOffset = offsetCents / 100.0;
                point.curveType = PitchPoint::CurveType::Linear;
                note.pitchCurve.push_back(point);
            }
        };

        if (forceStablePaSa && (matched == 0 || matched == 700))
        {
            applyFlat(correction);
            continue;
        }

        if (note.velocity < 1.0f / 3.0f)
        {
            applyFlat(correction);
            continue;
        }

        GamakaQuery query;
        query.svarasthana = matched;
        query.durationBeats = (float)note.durationBeats;
        query.excursionCents = quietRange + (loudRange - quietRange)
                    * std::clamp((note.velocity - 1.0f / 3.0f) / (2.0f / 3.0f), 0.0f, 1.0f);

        if (i > 0)
            query.previousInterval = (notes[i - 1].noteNumber - note.noteNumber) * 100;
        if (i + 1 < notes.size())
            query.nextInterval = (notes[i + 1].noteNumber - note.noteNumber) * 100;

        const auto grid = synthesize(query);
        if (grid.empty())
        {
            applyFlat(correction);
            continue;
        }

        for (int index : douglasPeuckerIndices(grid, simplifyCents))
        {
            PitchPoint point;
            // Beats, not normalized time: PitchPoint::time is compared against a
            // beat offset from the note start when the curve is evaluated.
            point.time = note.durationBeats * (double)index / (double)(gridSize - 1);
            point.pitchOffset = (double)(grid[(size_t)index] + correction) / 100.0;
            note.pitchCurve.push_back(point);
        }
    }
}
