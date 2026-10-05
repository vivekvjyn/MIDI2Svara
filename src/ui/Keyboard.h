#pragma once
#include <JuceHeader.h>
#include <set>
#include "LookAndFeel.h"

class Keyboard : public Component
{
public:
    Keyboard();

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;

    void setViewRange(double lowestNote, double highestNote);

    int noteAtY(int y) const;
    std::vector<int> ragaRows() const;

    void setActiveNotes(const std::set<int>& notes);
    void setRagaIntervals(const std::set<int>& intervals) { ragaIntervals = intervals; repaint(); }
    void setRootNote(int midiNote) { rootMidiNote = midiNote; repaint(); }

    std::function<void(int note, float velocity, bool isDown)> onKeyAction;
    std::function<void(const MouseEvent&, const MouseWheelDetails&)> onMouseWheel;

private:
    double lowestVisibleNote = 36.0;
    double highestVisibleNote = 84.0;
    std::set<int> activeNotes;
    int mouseDownNote = -1;
    std::set<int> ragaIntervals;
    int rootMidiNote = 60;

    void releaseMouseNote();
    bool isInRaga(int note) const;
};
