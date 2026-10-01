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

int Keyboard::noteAtY(int y) const
{
    double noteRange = highestVisibleNote - lowestVisibleNote;
    if (noteRange <= 0.0) return (int)lowestVisibleNote;
    int note = (int)(highestVisibleNote - (double)y / (double)getHeight() * noteRange);
    return jlimit((int)std::floor(lowestVisibleNote), (int)std::ceil(highestVisibleNote), note);
}

float Keyboard::yForNote(double note) const
{
    double noteRange = highestVisibleNote - lowestVisibleNote;
    if (noteRange <= 0.0) return 0.0f;
    return (float)((highestVisibleNote - note) / noteRange * (double)getHeight());
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

void Keyboard::paint(Graphics& g)
{
    g.fillAll(FPColours::background);

    int h = getHeight();
    int w = getWidth();
    double noteRange = highestVisibleNote - lowestVisibleNote;
    if (noteRange <= 0.0) return;

    noteHeight = jmax(1, (int)(h / noteRange));

    int lowInt = (int)std::floor(lowestVisibleNote);
    int highInt = (int)std::ceil(highestVisibleNote);

    const float blackKeyWidth = w * 0.55f;
    const float blackKeyOffset = (w - blackKeyWidth) * 0.5f;
    const float blackKeyHeightRatio = 0.65f;

    for (int note = lowInt; note <= highInt; ++note)
    {
        if (isBlackKey(note)) continue;

        float y = yForNote((double)note);
        float nextY = yForNote((double)(note + 1));
        float keyH = y - nextY;

        bool active = activeNotes.count(note) > 0;
        bool pressed = (note == mouseDownNote);
        bool disabled = !ragaIntervals.empty() && ragaIntervals.count(((note - rootMidiNote) % 12 + 12) % 12) == 0;

        Colour keyColour;
        if (disabled)
            keyColour = Colour(0xff282830);
        else if (pressed)
            keyColour = Colour(0xff8888a0);
        else if (active)
            keyColour = Colour(0xff7090b0);
        else
            keyColour = Colour(0xffd8d8d0);

        g.setColour(keyColour);
        g.fillRect(0.0f, nextY, (float)w, keyH);

        if (disabled)
        {
            g.setColour(Colour(0x10ffffff));
            g.fillRect(0.0f, nextY, (float)w, keyH);
        }

        if (active || pressed)
        {
            g.setColour(FPColours::accentCyan.withAlpha(pressed ? 0.35f : 0.25f));
            g.fillRect(0.0f, nextY, (float)w, keyH);
        }

        g.setColour(FPColours::pianoKeyBorder);
        g.drawHorizontalLine((int)y, 0.0f, (float)w);

        if (note % 12 == 0)
        {
            g.setColour(disabled ? FPColours::textDim : FPColours::text);
            g.setFont(FontOptions(10.0f));
            g.drawText(getNoteName(note), 2, (int)nextY, w - 4, (int)keyH, Justification::centredLeft);
        }
    }

    for (int note = lowInt; note <= highInt; ++note)
    {
        if (!isBlackKey(note)) continue;

        float y = yForNote((double)note);
        float nextY = yForNote((double)(note + 1));
        float fullKeyH = y - nextY;
        float keyH = fullKeyH * blackKeyHeightRatio;
        float keyY = nextY;

        g.setColour(Colour(0xff0a0a0a));
        g.fillRect(blackKeyOffset, keyY, blackKeyWidth, keyH);
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
