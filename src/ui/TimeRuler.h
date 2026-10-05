#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"

class TimeRuler : public Component
{
public:
    TimeRuler();

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;

    void setViewRange(double startBeat, double endBeat);
    void setPlayheadPosition(double beat) { playheadBeat = beat; repaint(); }
    double getBeatAtX(int x) const;
    int getXForBeat(double beat) const;

    std::function<void(double)> onPositionClicked;

private:
    double viewStartBeat = 0.0;
    double viewEndBeat = 16.0;
    double playheadBeat = 0.0;
    int beatsPerBar = 4;
};
