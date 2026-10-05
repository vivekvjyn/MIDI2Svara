#include "dsp/Engine.h"
#include "dsp/Shapes.h"
#include "GoldenCurves.h"
#include <gtest/gtest.h>
#include <cmath>
#include <limits>

namespace
{
    LookupRow makeRow(const char* gamaka, double top, double bottom, int previousInterval = 0)
    {
        LookupRow row;
        row.svara = "S";
        row.previous = "R";
        row.next = "G";
        row.gamaka = gamaka;
        row.previousInterval = previousInterval;
        row.top = top;
        row.bottom = bottom;
        return row;
    }

    NoteSequence makeSequence()
    {
        NoteSequence sequence;
        const int pitches[] = { 60, 62, 64, 67 };
        for (int i = 0; i < 4; ++i)
        {
            NoteData note;
            note.noteNumber = pitches[i];
            note.startBeat = (double)i;
            note.durationBeats = 1.0;
            sequence.addNote(note);
        }
        return sequence;
    }
}

TEST(Shapes, SthiraSlidesFromStartToSvara)
{
    auto row = makeRow("sthira", 100.0, -100.0, -50);
    auto curve = Shapes::curve(row, 1.0, 0.0, std::numeric_limits<double>::quiet_NaN());

    ASSERT_EQ(curve.size(), 512u);
    EXPECT_NEAR(curve.front(), -50.0, 1e-9);
    EXPECT_NEAR(curve.back(), 0.0, 1e-9);
}

TEST(Shapes, EveryGamakaIsFiniteAnd512Points)
{
    const char* types[] = { "sthira", "jaru", "nokku", "kampita", "andola", "vali",
                            "sphurita", "tripuchcha", "ahata", "pratyahata", "khandippu",
                            "odukkal", "janta", "orikai", "ravai" };

    for (const char* type : types)
    {
        auto row = makeRow(type, 180.0, -260.0, 200);
        auto curve = Shapes::curve(row, 1.5, 0.0, std::numeric_limits<double>::quiet_NaN());
        ASSERT_EQ(curve.size(), 512u) << type;
        for (double value : curve)
            ASSERT_TRUE(std::isfinite(value)) << type;
    }
}

TEST(Shapes, KampitaReachesTheExcursion)
{
    auto row = makeRow("kampita", 200.0, -100.0);
    auto curve = Shapes::curve(row, 2.0, 0.0, std::numeric_limits<double>::quiet_NaN());

    auto bounds = std::minmax_element(curve.begin(), curve.end());
    EXPECT_LT(*bounds.first, -100.0);
    EXPECT_GT(*bounds.second, 200.0);
}

TEST(Shapes, TransformMatchesLookupRules)
{
    LookupRow row = makeRow("kampita", 130.0, -70.0);
    Shapes::transformRow("kalyani", row, 0.0);
    EXPECT_NEAR(row.top, 152.1, 1e-9);
    EXPECT_NEAR(row.bottom, -81.9, 1e-9);

    row = makeRow("kampita", 130.0, -70.0);
    Shapes::transformRow("sahana", row, 0.0);
    EXPECT_NEAR(row.top, 134.0, 1e-9);

    row = makeRow("jaru", 130.0, -70.0);
    Shapes::transformRow("kalyani", row, 0.0);
    EXPECT_NEAR(row.top, 260.0, 1e-9);

    row = makeRow("sthira", 500.0, -100.0);
    Shapes::transformRow("sri", row, 300.0);
    EXPECT_NEAR(row.top, 300.0 + 1.3 * 200.0, 1e-9);
}

TEST(Shapes, MatchesThePythonGenerator)
{
    for (int c = 0; c < goldenCaseCount; ++c)
    {
        const auto& golden = goldenCases[c];

        LookupRow row;
        row.svara = "S";
        row.previous = "R";
        row.next = "G";
        row.gamaka = golden.gamaka;
        row.previousInterval = golden.previousInterval;
        row.top = golden.top;
        row.bottom = golden.bottom;
        Shapes::transformRow(golden.raga, row, golden.here);

        auto curve = Shapes::curve(row, golden.length, golden.here, golden.start);
        ASSERT_EQ(curve.size(), 512u) << golden.raga << " " << golden.gamaka;
        for (int k = 0; k < 8; ++k)
            EXPECT_NEAR(curve[(size_t)goldenIndices[k]], golden.expected[k], 1.0e-6)
                << golden.raga << " " << golden.gamaka << " length " << golden.length
                << " index " << goldenIndices[k];
    }
}

TEST(Shapes, ConnectJoinsAcrossBoundary)
{
    std::vector<NoteData> notes(2);
    notes[0].startBeat = 0.0;
    notes[0].durationBeats = 1.0;
    notes[1].startBeat = 1.0;
    notes[1].durationBeats = 1.0;

    PitchPoint flat;
    flat.time = 0.0;
    flat.curveType = PitchPoint::CurveType::Linear;
    flat.pitchOffset = 0.0;
    notes[0].pitchCurve.push_back(flat);
    flat.pitchOffset = 100.0;
    notes[1].pitchCurve.push_back(flat);

    Shapes::connect(notes);

    EXPECT_NEAR(notes[1].pitchCurve[0].pitchOffset, 50.0, 1e-6);
    EXPECT_NEAR(notes[0].pitchCurve[0].pitchOffset, 0.0, 1e-9);
}

TEST(Engine, LoadsEveryRaga)
{
    for (const auto& raga : Engine::getAvailableRagas())
    {
        Engine engine;
        ASSERT_TRUE(engine.loadRaga(raga)) << raga;
        ASSERT_FALSE(engine.getRows().empty()) << raga;

        for (const auto& row : engine.getRows())
        {
            ASSERT_TRUE(engine.getPositions().count(row.svara) > 0) << raga << " " << row.svara;
            ASSERT_TRUE(std::isfinite(row.top)) << raga;
            ASSERT_TRUE(std::isfinite(row.bottom)) << raga;
        }
    }
}

TEST(Engine, PositionsMatchTheScale)
{
    Engine engine;
    ASSERT_TRUE(engine.loadRaga("kalyani"));
    const auto& positions = engine.getPositions();

    EXPECT_DOUBLE_EQ(positions.at("S"), 0.0);
    EXPECT_DOUBLE_EQ(positions.at("M"), 600.0);
    EXPECT_DOUBLE_EQ(positions.at("N"), 1100.0);
    EXPECT_DOUBLE_EQ(positions.at("D_"), -300.0);
    EXPECT_DOUBLE_EQ(positions.at("S^^"), 2400.0);
}

TEST(Engine, OrnamentsEveryNote)
{
    Engine engine;
    ASSERT_TRUE(engine.loadRaga("kalyani"));
    engine.setRootNote(60);

    auto sequence = makeSequence();
    engine.applyExpression(sequence);

    for (int i = 0; i < sequence.getNumNotes(); ++i)
    {
        const auto& note = sequence.getNote(i);
        ASSERT_EQ(note.pitchCurve.size(), 512u) << i;
        EXPECT_DOUBLE_EQ(note.pitchCurve.front().time, 0.0);
        EXPECT_NEAR(note.pitchCurve.back().time, note.durationBeats * 511.0 / 512.0, 1e-9);
        for (const auto& point : note.pitchCurve)
        {
            EXPECT_TRUE(std::isfinite(point.pitchOffset));
            EXPECT_LT(std::abs(point.pitchOffset), 20.0);
        }
        for (size_t k = 1; k < note.pitchCurve.size(); ++k)
            EXPECT_LE(note.pitchCurve[k - 1].time, note.pitchCurve[k].time);
    }
}

TEST(Engine, StablePaSaStaysFlat)
{
    Engine engine;
    ASSERT_TRUE(engine.loadRaga("kalyani"));
    engine.setRootNote(60);

    auto sequence = makeSequence();
    engine.applyExpression(sequence, true, 0.0f);

    EXPECT_TRUE(sequence.getNote(0).pitchCurve.empty());
    EXPECT_TRUE(sequence.getNote(3).pitchCurve.empty());
    EXPECT_EQ(sequence.getNote(1).pitchCurve.size(), 512u);
}

TEST(Engine, PitchCorrectionPullsOffScaleNotes)
{
    Engine engine;
    ASSERT_TRUE(engine.loadRaga("kalyani"));
    engine.setRootNote(60);

    NoteSequence sequence;
    NoteData note;
    note.noteNumber = 61;
    note.durationBeats = 1.0;
    sequence.addNote(note);

    engine.applyExpression(sequence, true, 1.0f);

    ASSERT_EQ(sequence.getNote(0).pitchCurve.size(), 1u);
    EXPECT_NEAR(sequence.getNote(0).pitchCurve[0].pitchOffset, -1.0, 1e-9);
}
