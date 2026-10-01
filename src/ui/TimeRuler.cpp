#include "TimeRuler.h"


TimeRuler::TimeRuler()
{
    setSize(800, 25);
}

void TimeRuler::setViewRange(double startBeat, double endBeat)
{
    viewStartBeat = startBeat;
    viewEndBeat = endBeat;
    repaint();
}

double TimeRuler::getBeatAtX(int x) const
{
    double range = viewEndBeat - viewStartBeat;
    return viewStartBeat + (double)x / (double)getWidth() * range;
}

int TimeRuler::getXForBeat(double beat) const
{
    double range = viewEndBeat - viewStartBeat;
    if (range <= 0.0) return 0;
    return (int)((beat - viewStartBeat) / range * getWidth());
}

void TimeRuler::paint(Graphics& g)
{
    g.fillAll(FPColours::toolbarBg);

    double range = viewEndBeat - viewStartBeat;
    if (range <= 0.0) return;

    int firstBeat = (int)std::floor(viewStartBeat);
    int lastBeat = (int)std::ceil(viewEndBeat);

    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        int x = getXForBeat((double)beat);
        bool isBarLine = (beat % beatsPerBar == 0);

        g.setColour(isBarLine ? FPColours::text : FPColours::textDim);

        if (isBarLine)
        {
            g.drawVerticalLine(x, 0.0f, (float)getHeight());
            int barNum = beat / beatsPerBar + 1;
            g.setFont(FontOptions(11.0f));
            g.drawText(String(barNum), x + 3, 0, 30, getHeight(), Justification::centredLeft);
        }
        else
        {
            g.drawVerticalLine(x, (float)getHeight() * 0.6f, (float)getHeight());
        }
    }

    int phX = getXForBeat(playheadBeat);
    g.setColour(FPColours::playhead);
    g.drawVerticalLine(phX, 0.0f, (float)getHeight());
    g.fillRect(phX - 4, 0, 8, 6);
}

void TimeRuler::mouseDown(const MouseEvent& e)
{
    double beat = getBeatAtX(e.x);
    if (onPositionClicked)
        onPositionClicked(beat);
}
