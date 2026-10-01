#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"
#include "LookAndFeel.h"

class NoteComponent
{
public:
    static void drawNote(Graphics& g, const NoteData& note,
                         Rectangle<float> bounds, bool isSelected,
                         float alphaMul = 1.0f);

    static void drawPitchCurve(Graphics& g, const NoteData& note,
                                Rectangle<float> bounds,
                                float pixelsPerSemitone,
                                int hoveredSegment = -1,
                                float alphaMul = 1.0f);
};
