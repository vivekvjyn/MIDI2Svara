#include "Scrollbar.h"


Rectangle<float> Scrollbar::getThumbRect() const
{
    auto b = getLocalBounds().toFloat();
    if (orientation == Orientation::Horizontal)
    {
        float x1 = b.getX() + (float)(tStart * b.getWidth());
        float x2 = b.getX() + (float)(tEnd * b.getWidth());
        return { x1, b.getY(), x2 - x1, b.getHeight() };
    }
    else
    {
        float y1 = b.getY() + (float)(tStart * b.getHeight());
        float y2 = b.getY() + (float)(tEnd * b.getHeight());
        return { b.getX(), y1, b.getWidth(), y2 - y1 };
    }
}

void Scrollbar::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour(FPColours::surface.darker(0.3f));
    g.fillRect(b);

    auto thumb = getThumbRect();
    g.setColour(FPColours::textDim.withAlpha(0.35f));
    g.fillRoundedRectangle(thumb.reduced(1.0f), 3.0f);

    if (dragging)
    {
        g.setColour(FPColours::accent.withAlpha(0.6f));
        g.drawRoundedRectangle(thumb.reduced(1.0f), 3.0f, 1.0f);
    }
}

void Scrollbar::mouseDown(const MouseEvent& e)
{
    auto thumb = getThumbRect();

    if (orientation == Orientation::Horizontal)
    {
        if (thumb.contains((float)e.x, (float)e.y))
        {
            dragging = true;
            dragOffset = mouseToNorm((float)e.x) - tStart;
        }
        else
        {
            
            double thumbSize = tEnd - tStart;
            double newStart = mouseToNorm((float)e.x) - thumbSize * 0.5;
            newStart = jlimit(0.0, 1.0 - thumbSize, newStart);
            tStart = newStart;
            tEnd = newStart + thumbSize;
            dragging = true;
            dragOffset = thumbSize * 0.5;
            if (onScroll) onScroll(tStart, tEnd);
            repaint();
        }
    }
    else
    {
        if (thumb.contains((float)e.x, (float)e.y))
        {
            dragging = true;
            dragOffset = mouseToNorm((float)e.y) - tStart;
        }
        else
        {
            double thumbSize = tEnd - tStart;
            double newStart = mouseToNorm((float)e.y) - thumbSize * 0.5;
            newStart = jlimit(0.0, 1.0 - thumbSize, newStart);
            tStart = newStart;
            tEnd = newStart + thumbSize;
            dragging = true;
            dragOffset = thumbSize * 0.5;
            if (onScroll) onScroll(tStart, tEnd);
            repaint();
        }
    }
}

void Scrollbar::mouseDrag(const MouseEvent& e)
{
    if (!dragging) return;

    float pos = (orientation == Orientation::Horizontal) ? (float)e.x : (float)e.y;
    double norm = mouseToNorm(pos);
    double thumbSize = tEnd - tStart;
    double newStart = norm - dragOffset;
    newStart = jlimit(0.0, 1.0 - thumbSize, newStart);

    tStart = newStart;
    tEnd = newStart + thumbSize;

    if (onScroll) onScroll(tStart, tEnd);
    repaint();
}

void Scrollbar::mouseUp(const MouseEvent&)
{
    dragging = false;
    repaint();
}

void ZoomFader::paint(Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(FPColours::background.withAlpha(0.75f));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(FPColours::gridLine.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    auto b = bounds.reduced(4.0f);

    g.setColour(FPColours::textDim.withAlpha(0.45f));
    if (orientation == Orientation::Horizontal)
    {
        float cy = b.getCentreY();
        g.drawLine(b.getX(), cy, b.getRight(), cy, 1.5f);
    }
    else
    {
        float cx = b.getCentreX();
        g.drawLine(cx, b.getY(), cx, b.getBottom(), 1.5f);
    }

    float dotSize = 8.0f;
    if (orientation == Orientation::Horizontal)
    {
        float x = b.getX() + (float)zoomLevel * b.getWidth();
        g.setColour(FPColours::accent);
        g.fillEllipse(x - dotSize * 0.5f, b.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
    }
    else
    {
        float y = b.getBottom() - (float)zoomLevel * b.getHeight();
        g.setColour(FPColours::accent);
        g.fillEllipse(b.getCentreX() - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
    }
}

void ZoomFader::mouseDown(const MouseEvent& e)
{
    mouseDrag(e);
}

void ZoomFader::mouseDrag(const MouseEvent& e)
{
    double level;
    auto b = getLocalBounds().toFloat().reduced(2.0f);

    if (orientation == Orientation::Horizontal)
    {
        level = ((double)e.x - (double)b.getX()) / (double)b.getWidth();
    }
    else
    {
        
        level = ((double)b.getBottom() - (double)e.y) / (double)b.getHeight();
    }

    zoomLevel = jlimit(0.0, 1.0, level);
    if (onZoomChanged) onZoomChanged(zoomLevel);
    repaint();
}
