#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"
#include "LookAndFeel.h"

class PianoRoll;

class NoteComponent
{
public:
    static void drawNote(Graphics& g, Rectangle<float> bounds, bool isSelected,
                         Colour colour);

    static void drawPitchCurve(Graphics& g, const NoteData& note,
                               Rectangle<float> bounds,
                               const PianoRoll& roll,
                               int hoveredSegment = -1,
                               float alphaMul = 1.0f);
};
