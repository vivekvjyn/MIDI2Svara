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
    int getNoteHeight() const { return noteHeight; }
    double getLowestNote() const { return lowestVisibleNote; }
    double getHighestNote() const { return highestVisibleNote; }

    int noteAtY(int y) const;
    float yForNote(double note) const;

    void setActiveNotes(const std::set<int>& notes);
    void setRagaIntervals(const std::set<int>& intervals) { ragaIntervals = intervals; repaint(); }
    void setRootNote(int midiNote) { rootMidiNote = midiNote; repaint(); }
    void clearRagaIntervals() { ragaIntervals.clear(); repaint(); }

    std::function<void(int note, float velocity, bool isDown)> onKeyAction;
    std::function<void(const MouseEvent&, const MouseWheelDetails&)> onMouseWheel;

private:
    double lowestVisibleNote = 36.0;
    double highestVisibleNote = 84.0;
    int noteHeight = 16;
    std::set<int> activeNotes;
    int mouseDownNote = -1;
    std::set<int> ragaIntervals;
    int rootMidiNote = 60;

    void releaseMouseNote();

    static bool isBlackKey(int note)
    {
        int n = note % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    static String getNoteName(int note)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (note / 12) - 1;
        return String(names[note % 12]) + String(octave);
    }
};
