#include "CurveDrawing.h"
#include "../dsp/PitchCurve.h"
#include <algorithm>


void CurveDrawing::beginDrawing(NoteData* note, double startTime, float startPitch)
{
    targetNote = note;
    drawing = true;
    drawnPoints.clear();

    PitchPoint pt;
    pt.time = startTime;
    pt.pitchOffset = startPitch;
    pt.curveType = PitchPoint::CurveType::Smooth;
    drawnPoints.push_back(pt);
    lastTime = startTime;
    cursorTime = startTime;
    cursorPitch = startPitch;
}

void CurveDrawing::continueDrawing(double time, float pitch)
{
    if (!drawing || targetNote == nullptr) return;

    cursorTime = time;
    cursorPitch = pitch;

    if (time <= lastTime) return;

    if (time - lastTime < 0.005) return;

    PitchPoint pt;
    pt.time = time;
    pt.pitchOffset = pitch;
    pt.curveType = PitchPoint::CurveType::Smooth;
    drawnPoints.push_back(pt);
    lastTime = time;
}

void CurveDrawing::endDrawing()
{
    if (!drawing || targetNote == nullptr) return;

    if (mode == Mode::Freehand)
    {
        
        if (drawnPoints.size() > 50)
        {
            std::vector<PitchPoint> decimated;
            size_t step = drawnPoints.size() / 40;
            if (step < 1) step = 1;

            for (size_t i = 0; i < drawnPoints.size(); i += step)
                decimated.push_back(drawnPoints[i]);

            if (std::abs(decimated.back().time - drawnPoints.back().time) > 0.0001f)
                decimated.push_back(drawnPoints.back());

            drawnPoints = decimated;
        }

        PitchCurveInterpolator::smooth(drawnPoints, 0.3f);

        if (!drawnPoints.empty())
        {
            double maxTime = drawnPoints.back().time;
            if (maxTime > targetNote->durationBeats)
                targetNote->durationBeats = maxTime;
        }

        targetNote->pitchCurve = drawnPoints;
    }
    else if (mode == Mode::ClickDraw && !drawnPoints.empty())
    {
        
        for (auto& pt : drawnPoints)
            targetNote->pitchCurve.push_back(pt);

        std::sort(targetNote->pitchCurve.begin(), targetNote->pitchCurve.end(),
                  [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });
    }
    else if (mode == Mode::Erase && targetNote != nullptr)
    {
        
        if (!drawnPoints.empty())
        {
            double startT = drawnPoints.front().time;
            double endT = drawnPoints.back().time;
            if (startT > endT) std::swap(startT, endT);

            auto& curve = targetNote->pitchCurve;
            curve.erase(
                std::remove_if(curve.begin(), curve.end(),
                    [startT, endT](const PitchPoint& p) {
                        return p.time >= startT && p.time <= endT;
                    }),
                curve.end());
        }
    }

    drawing = false;
    drawnPoints.clear();
    targetNote = nullptr;
}
