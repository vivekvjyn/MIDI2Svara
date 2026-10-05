#pragma once
#include <JuceHeader.h>
#include "NoteData.h"
#include <vector>

struct LookupRow
{
    String svara;
    String previous;
    String next;
    String gamaka;
    int previousInterval = 0;
    int nextInterval = 0;
    double top = 0.0;
    double bottom = 0.0;
};

namespace Shapes
{
    void transformRow(const String& raga, LookupRow& row, double here);

    std::vector<double> curve(const LookupRow& row, double lengthBeats,
                              double here, double start);

    void connect(std::vector<NoteData>& notes);
}
