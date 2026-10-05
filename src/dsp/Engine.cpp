#include "Engine.h"
#include <BinaryData.h>
#include <rapidjson/document.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
    int floorMod(int value, int modulus)
    {
        int result = value % modulus;
        return (result < 0) ? result + modulus : result;
    }

    int shortestCentsBetween(int a, int b)
    {
        int diff = floorMod(a - b, 1200);
        return (diff > 600) ? diff - 1200 : diff;
    }

    struct Letter { const char* name; double cents; };

    const std::map<String, std::vector<Letter>>& letterTables()
    {
        static const std::map<String, std::vector<Letter>> tables = {
            { "abhogi",  { { "S", 0 }, { "R", 200 }, { "G", 300 }, { "M", 500 }, { "D", 900 } } },
            { "begada",  { { "S", 0 }, { "R", 200 }, { "G", 400 }, { "M", 500 }, { "P", 700 }, { "D", 900 }, { "N", 1100 } } },
            { "kalyani", { { "S", 0 }, { "R", 200 }, { "G", 400 }, { "M", 600 }, { "P", 700 }, { "D", 900 }, { "N", 1100 } } },
            { "mohanam", { { "S", 0 }, { "R", 200 }, { "G", 400 }, { "P", 700 }, { "D", 900 } } },
            { "sahana",  { { "S", 0 }, { "R", 200 }, { "G", 400 }, { "M", 500 }, { "P", 700 }, { "D", 900 }, { "N", 1000 } } },
            { "saveri",  { { "S", 0 }, { "R", 100 }, { "G", 400 }, { "M", 500 }, { "P", 700 }, { "D", 800 }, { "N", 1100 } } },
            { "sri",     { { "S", 0 }, { "R", 200 }, { "G", 300 }, { "M", 500 }, { "P", 700 }, { "D", 900 }, { "N", 1000 } } },
        };
        return tables;
    }

    void buildScale(const String& raga,
                    std::map<String, double>& positions,
                    std::vector<std::pair<String, double>>& byPitch,
                    std::vector<int>& folded)
    {
        positions.clear();
        byPitch.clear();
        folded.clear();

        auto table = letterTables().find(raga);
        if (table == letterTables().end())
            return;

        for (const auto& letter : table->second)
        {
            for (int k = -2; k <= 2; ++k)
            {
                String name = letter.name;
                if (k < 0)
                    name += String::repeatedString("_", -k);
                else if (k > 0)
                    name += String::repeatedString("^", k);
                double cents = letter.cents + 1200.0 * k;
                positions[name] = cents;
                byPitch.push_back({ name, cents });
            }
        }

        std::sort(byPitch.begin(), byPitch.end(),
                  [](const std::pair<String, double>& a, const std::pair<String, double>& b)
                  { return a.second < b.second; });

        for (const auto& entry : byPitch)
        {
            int value = floorMod((int)std::lround(entry.second), 1200);
            if (std::find(folded.begin(), folded.end(), value) == folded.end())
                folded.push_back(value);
        }
    }

    int closestOnScale(const std::vector<int>& scale, int value)
    {
        int best = scale.front();
        int bestDistance = std::numeric_limits<int>::max();
        for (int candidate : scale)
        {
            int distance = std::abs(shortestCentsBetween(candidate, value));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
        return best;
    }

    String keyOf(const String& svara, const String& previous, const String& next)
    {
        return svara + "|" + previous + "|" + next;
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

Engine::Engine()
{
    auto ragas = getAvailableRagas();
    if (!ragas.isEmpty())
        loadRaga(ragas[0]);
}

StringArray Engine::getAvailableRagas()
{
    return { "abhogi", "begada", "kalyani", "mohanam", "sahana", "saveri", "sri" };
}

const std::map<String, std::set<int>>& Engine::getRagaIntervals()
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

int Engine::getRagaVadi(const String& ragaName)
{
    static const std::map<String, int> vadi = {
        { "abhogi",  3 },
        { "begada",  7 },
        { "kalyani", 4 },
        { "mohanam", 4 },
        { "sahana",  7 },
        { "saveri",  1 },
        { "sri",     2 },
    };

    auto it = vadi.find(ragaName);
    return it == vadi.end() ? -1 : it->second;
}

bool Engine::isNoteInRaga(int midiNote, const String& ragaName, int rootNote)
{
    const auto& intervals = getRagaIntervals();
    auto it = intervals.find(ragaName);
    if (it == intervals.end())
        return true;
    return it->second.count(floorMod(midiNote - rootNote, 12)) > 0;
}

bool Engine::loadRaga(const String& ragaName)
{
    loadRagaData(ragaName);
    currentRaga = ragaName;
    return !rows.empty();
}

void Engine::loadRagaData(const String& ragaName)
{
    rows.clear();
    present.clear();
    buildScale(ragaName, positions, byPitch, folded);
    if (positions.empty())
        return;

    int size = 0;
    const char* data = findRagaResource(ragaName, size);
    if (data == nullptr || size <= 0)
        return;

    rapidjson::Document doc;
    doc.Parse(data, (size_t)size);
    if (doc.HasParseError() || !doc.IsArray())
        return;

    rows.reserve(doc.Size());

    for (const auto& entry : doc.GetArray())
    {
        if (!entry.IsObject() || !entry.HasMember("svara") || !entry.HasMember("gamaka"))
            continue;
        if (!entry.HasMember("previous") || !entry.HasMember("next") || !entry.HasMember("params"))
            continue;

        LookupRow row;
        row.svara = entry["svara"].GetString();
        row.previous = entry["previous"].GetString();
        row.next = entry["next"].GetString();
        row.gamaka = entry["gamaka"].GetString();
        row.previousInterval = entry["previousInterval"].IsInt() ? entry["previousInterval"].GetInt() : 0;
        row.nextInterval = entry["nextInterval"].IsInt() ? entry["nextInterval"].GetInt() : 0;

        const auto& params = entry["params"];
        if (!params.IsObject() || !params.HasMember("top") || !params.HasMember("bottom"))
            continue;
        row.top = params["top"].GetDouble();
        row.bottom = params["bottom"].GetDouble();

        auto here = positions.find(row.svara);
        if (here == positions.end())
            continue;

        Shapes::transformRow(ragaName, row, here->second);
        present[keyOf(row.svara, row.previous, row.next)] = rows.size();
        rows.push_back(std::move(row));
    }
}

String Engine::nearestSvara(double cents) const
{
    if (byPitch.empty())
        return {};

    double bestDistance = std::numeric_limits<double>::max();
    String best = byPitch.front().first;
    for (const auto& entry : byPitch)
    {
        double distance = std::abs(entry.second - cents);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = entry.first;
        }
    }
    return best;
}

const LookupRow& Engine::selectRow(const String& svara, const String* previous, const String* next)
{
    const String key = keyOf(svara, previous ? *previous : String(), next ? *next : String());
    auto exact = present.find(key);
    if (exact != present.end())
        return rows[exact->second];

    std::vector<size_t> pool;
    auto pick = [this, &pool]() -> const LookupRow&
    {
        std::uniform_int_distribution<size_t> choice(0, pool.size() - 1);
        return rows[choice(rng)];
    };

    if (previous != nullptr)
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].svara == svara && rows[i].previous == *previous)
                pool.push_back(i);
        if (!pool.empty())
            return pick();
        pool.clear();
    }

    if (next != nullptr)
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].svara == svara && rows[i].next == *next)
                pool.push_back(i);
        if (!pool.empty())
            return pick();
        pool.clear();
    }

    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].svara == svara)
            pool.push_back(i);
    if (pool.empty())
    {
        pool.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
            pool.push_back(i);
    }
    return pick();
}

void Engine::applyExpression(NoteSequence& sequence, bool forceStablePaSa, float pitchCorrection)
{
    auto& notes = sequence.getAllNotes();
    if (notes.empty() || rows.empty() || folded.empty())
        return;

    std::sort(notes.begin(), notes.end(),
              [](const NoteData& a, const NoteData& b) { return a.startBeat < b.startBeat; });

    std::vector<String> svaras(notes.size());
    for (size_t i = 0; i < notes.size(); ++i)
        svaras[i] = nearestSvara((notes[i].noteNumber - rootMidiNote) * 100.0);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& note = notes[i];
        note.pitchCurve.clear();
        if (note.durationBeats <= 0.0)
            continue;

        const String* previous = (i > 0) ? &svaras[i - 1] : nullptr;
        const String* next = (i + 1 < notes.size()) ? &svaras[i + 1] : nullptr;

        const String key = keyOf(svaras[i], previous ? *previous : String(), next ? *next : String());
        const bool exact = present.find(key) != present.end();
        const double here = positions.at(svaras[i]);
        const double origin = (exact && previous != nullptr)
            ? positions.at(*previous)
            : std::numeric_limits<double>::quiet_NaN();

        const LookupRow& row = selectRow(svaras[i], previous, next);
        const auto shape = Shapes::curve(row, note.durationBeats, here, origin);

        const int foldedNote = floorMod((int)std::lround((note.noteNumber - rootMidiNote) * 100.0), 1200);
        const int matched = closestOnScale(folded, foldedNote);
        const float correction = pitchCorrection * (float)shortestCentsBetween(matched, foldedNote);

        if (forceStablePaSa && (matched == 0 || matched == 700))
        {
            if (std::abs(correction) > 1.0e-3f)
            {
                PitchPoint point;
                point.time = 0.0;
                point.pitchOffset = correction / 100.0;
                point.curveType = PitchPoint::CurveType::Linear;
                note.pitchCurve.push_back(point);
            }
            continue;
        }

        note.pitchCurve.reserve(shape.size());
        for (size_t k = 0; k < shape.size(); ++k)
        {
            PitchPoint point;
            point.time = note.durationBeats * (double)k / (double)shape.size();
            point.pitchOffset = (shape[k] - here + correction) / 100.0;
            note.pitchCurve.push_back(point);
        }
    }

    Shapes::connect(notes);
}
