#include "Keyboard.h"

Keyboard::Keyboard()
{
    setSize(60, 400);
}

void Keyboard::setViewRange(double lowest, double highest)
{
    lowestVisibleNote = lowest;
    highestVisibleNote = highest;
    repaint();
}

std::vector<int> Keyboard::ragaRows() const
{
    std::vector<int> rows;
    int lo = (int)std::floor(lowestVisibleNote);
    int hi = (int)std::ceil(highestVisibleNote);

    for (int n = lo; n <= hi; ++n)
        if (isInRaga(n))
            rows.push_back(n);

    if (rows.empty())
        for (int n = lo; n <= hi; ++n)
            rows.push_back(n);

    return rows;
}

int Keyboard::noteAtY(int y) const
{
    auto rows = ragaRows();
    int count = (int)rows.size();
    if (count == 0 || getHeight() <= 0) return (int)lowestVisibleNote;

    float rh = (float)getHeight() / (float)count;
    int idx = count - 1 - (int)((float)y / rh);
    return rows[(size_t)jlimit(0, count - 1, idx)];
}

void Keyboard::setActiveNotes(const std::set<int>& notes)
{
    if (notes != activeNotes)
    {
        activeNotes = notes;
        repaint();
    }
}

void Keyboard::releaseMouseNote()
{
    if (mouseDownNote >= 0)
    {
        if (onKeyAction)
            onKeyAction(mouseDownNote, 0.0f, false);
        mouseDownNote = -1;
        repaint();
    }
}

bool Keyboard::isInRaga(int note) const
{
    if (ragaIntervals.empty()) return true;
    int interval = ((note - rootMidiNote) % 12 + 12) % 12;
    return ragaIntervals.count(interval) > 0;
}

void Keyboard::paint(Graphics& g)
{
    g.fillAll(FPColours::background);

    int w = getWidth();
    int h = getHeight();
    auto rows = ragaRows();
    int count = (int)rows.size();
    if (count == 0 || h <= 0) return;

    float rh = (float)h / (float)count;
    const float keyInset = 2.0f;
    const float gap = 2.0f;

    int i = 0;
    for (int note : rows)
    {
        float top = (float)h - (float)(i + 1) * rh;
        Rectangle<float> keyRect(keyInset, top + gap * 0.5f,
                                 (float)w - keyInset * 2.0f, rh - gap);
        ++i;

        if (keyRect.getHeight() <= 1.0f) continue;

        bool pressed = (note == mouseDownNote);
        bool active = activeNotes.count(note) > 0;

        if (pressed)
            g.setColour(FPColours::text);
        else if (active)
            g.setColour(FPColours::buttonActive);
        else
            g.setColour(FPColours::pianoWhiteKey);
        g.fillRect(keyRect);

        g.setColour(pressed ? FPColours::surface : FPColours::textDim);
        g.setFont(FontOptions(11.0f));
        g.drawText(FPColours::swaraName(FPColours::relativeInterval(note, rootMidiNote)),
                   keyRect.reduced(7.0f, 0.0f), Justification::centredLeft);
    }
}

void Keyboard::mouseDown(const MouseEvent& e)
{
    int note = noteAtY(e.y);
    if (note >= 0)
    {
        mouseDownNote = note;
        if (onKeyAction) onKeyAction(note, 0.8f, true);
        repaint();
    }
}

void Keyboard::mouseDrag(const MouseEvent& e)
{
    if (!getLocalBounds().contains(e.getPosition()))
    {
        releaseMouseNote();
        return;
    }

    int note = noteAtY(e.y);
    if (note != mouseDownNote)
    {
        releaseMouseNote();
        if (note >= 0)
        {
            mouseDownNote = note;
            if (onKeyAction) onKeyAction(note, 0.8f, true);
            repaint();
        }
    }
}

void Keyboard::mouseUp(const MouseEvent&)
{
    releaseMouseNote();
}

void Keyboard::mouseExit(const MouseEvent&)
{
    releaseMouseNote();
}

void Keyboard::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (onMouseWheel)
        onMouseWheel(e, wheel);
}
