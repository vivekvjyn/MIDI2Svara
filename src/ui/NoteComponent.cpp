#include "NoteComponent.h"
#include "../dsp/PitchCurve.h"


void NoteComponent::drawNote(Graphics& g, const NoteData& note,
                              Rectangle<float> bounds, bool isSelected,
                              float alphaMul)
{
    auto colour = isSelected ? FPColours::noteBlockSelected : FPColours::noteBlock;

    if (!note.pitchCurve.empty())
    {
        float avgDeviation = 0.0f;
        for (auto& p : note.pitchCurve)
            avgDeviation += (float)std::abs(p.pitchOffset);
        avgDeviation /= (float)note.pitchCurve.size();

        float deviationFactor = jlimit(0.0f, 1.0f, avgDeviation / 2.0f);
        colour = colour.interpolatedWith(FPColours::pitchCurve, deviationFactor * 0.3f);
    }

    g.setColour(colour.withAlpha(0.8f * alphaMul));
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(colour.brighter(0.3f).withAlpha(alphaMul));
    g.drawRoundedRectangle(bounds, 3.0f, isSelected ? 2.0f : 1.0f);

    if (bounds.getWidth() > 30.0f)
    {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        g.setColour(FPColours::background.withAlpha(alphaMul));
        g.setFont(FontOptions(10.0f));
        g.drawText(noteNames[note.noteNumber % 12],
                   bounds.reduced(4, 0), Justification::centredLeft);
    }
}

void NoteComponent::drawPitchCurve(Graphics& g, const NoteData& note,
                                    Rectangle<float> bounds,
                                    float pixelsPerSemitone,
                                    int hoveredSegment,
                                    float alphaMul)
{
    if (note.pitchCurve.empty()) return;

    float noteCentreY = bounds.getCentreY();
    float noteWidth = bounds.getWidth();
    float duration = (float)note.durationBeats;

    int numSegments = (int)note.pitchCurve.size() - 1;

    if (numSegments < 1)
    {
        auto& pt = note.pitchCurve[0];
        float px = bounds.getX() + (float)(pt.time / duration) * noteWidth;
        float py = noteCentreY - (float)pt.pitchOffset * pixelsPerSemitone;

        if (px > bounds.getX() + 2)
        {
            g.setColour(FPColours::pitchCurve.withAlpha(0.55f * alphaMul));
            g.drawLine(bounds.getX(), py, px, py, 1.5f);
        }

        if (px < bounds.getRight() - 2)
        {
            g.setColour(FPColours::pitchCurve.withAlpha(0.55f * alphaMul));
            g.drawLine(px, py, bounds.getRight(), py, 1.5f);
        }

        g.setColour(FPColours::pitchCurveAlt.withAlpha(alphaMul));
        g.fillEllipse(px - 3, py - 3, 6, 6);
        return;
    }

    for (int seg = 0; seg < numSegments; ++seg)
    {
        auto& ptA = note.pitchCurve[(size_t)seg];
        auto& ptB = note.pitchCurve[(size_t)seg + 1];

        float xA = bounds.getX() + (float)(ptA.time / duration) * noteWidth;
        float xB = bounds.getX() + (float)(ptB.time / duration) * noteWidth;
        float segWidth = xB - xA;

        bool isHovered = (seg == hoveredSegment) || (hoveredSegment == -2);

        Path segPath;
        int steps = jmax(10, (int)(segWidth));
        
        if (ptA.vibrato.enabled)
            steps = jmax(steps, (int)(segWidth * 3));
        bool started = false;

        for (int i = 0; i <= steps; ++i)
        {
            float t = (float)i / (float)steps;
            double time = ptA.time + t * (ptB.time - ptA.time);
            float pitchOffset = PitchCurveInterpolator::interpolate(note.pitchCurve, time);

            float x = xA + t * segWidth;
            float y = noteCentreY - pitchOffset * pixelsPerSemitone;

            if (!started) { segPath.startNewSubPath(x, y); started = true; }
            else segPath.lineTo(x, y);
        }

        auto baseColour = isHovered ? FPColours::pitchCurveAlt : FPColours::pitchCurve;
        
        if (ptA.vibrato.enabled)
            baseColour = baseColour.interpolatedWith(FPColours::vibrato, 0.35f);
        g.setColour(baseColour.withAlpha((isHovered ? 0.45f : 0.3f) * alphaMul));
        g.strokePath(segPath, PathStrokeType(isHovered ? 5.0f : 4.0f, PathStrokeType::curved));

        g.setColour(baseColour.withAlpha(alphaMul));
        g.strokePath(segPath, PathStrokeType(isHovered ? 2.5f : 2.0f, PathStrokeType::curved));
    }

    {
        
        auto& first = note.pitchCurve.front();
        float fx = bounds.getX() + (float)(first.time / duration) * noteWidth;
        float fy = noteCentreY - (float)first.pitchOffset * pixelsPerSemitone;
        if (fx > bounds.getX() + 2)
        {
            g.setColour(FPColours::pitchCurve.withAlpha(0.55f * alphaMul));
            g.drawLine(bounds.getX(), fy, fx, fy, 1.5f);
        }

        auto& last = note.pitchCurve.back();
        float lx = bounds.getX() + (float)(last.time / duration) * noteWidth;
        float ly = noteCentreY - (float)last.pitchOffset * pixelsPerSemitone;
        if (lx < bounds.getRight() - 2)
        {
            g.setColour(FPColours::pitchCurve.withAlpha(0.55f * alphaMul));
            g.drawLine(lx, ly, bounds.getRight(), ly, 1.5f);
        }
    }

    for (auto& pt : note.pitchCurve)
    {
        float px = bounds.getX() + (float)(pt.time / duration) * noteWidth;
        float py = noteCentreY - (float)pt.pitchOffset * pixelsPerSemitone;

        g.setColour(FPColours::pitchCurveAlt.withAlpha(alphaMul));
        g.fillEllipse(px - 3, py - 3, 6, 6);

        if (std::abs(pt.curvature - 1.0f) > 0.05f)
        {
            float ringR = 5.0f + pt.curvature * 1.5f;
            g.setColour(FPColours::vibrato.withAlpha(0.35f * alphaMul));
            g.drawEllipse(px - ringR, py - ringR, ringR * 2, ringR * 2, 1.0f);
        }
    }
}
