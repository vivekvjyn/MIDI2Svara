#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"


class Scrollbar : public Component
{
public:
    enum class Orientation { Horizontal, Vertical };

    explicit Scrollbar(Orientation orient) : orientation(orient) {}

    
    
    void setThumbRange(double thumbStart, double thumbEnd)
    {
        tStart = jlimit(0.0, 1.0, thumbStart);
        tEnd   = jlimit(0.0, 1.0, thumbEnd);
        if (tEnd < tStart + 0.01) tEnd = tStart + 0.01;
        repaint();
    }

    
    std::function<void(double newStart, double newEnd)> onScroll;   

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

private:
    Orientation orientation;
    double tStart = 0.0, tEnd = 1.0;

    bool dragging = false;
    double dragOffset = 0.0;

    double mouseToNorm(float pos) const
    {
        float length = (orientation == Orientation::Horizontal) ? (float)getWidth() : (float)getHeight();
        return (double)pos / (double)jmax(1.0f, length);
    }

    Rectangle<float> getThumbRect() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Scrollbar)
};


class ZoomFader : public Component
{
public:
    enum class Orientation { Horizontal, Vertical };

    explicit ZoomFader(Orientation orient) : orientation(orient) {}

    
    void setZoomLevel(double level)
    {
        zoomLevel = jlimit(0.0, 1.0, level);
        repaint();
    }

    double getZoomLevel() const { return zoomLevel; }

    std::function<void(double newZoom)> onZoomChanged;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;

private:
    Orientation orientation;
    double zoomLevel = 0.3;

    double mouseToNorm(float pos) const
    {
        float length = (orientation == Orientation::Horizontal) ? (float)getWidth() : (float)getHeight();
        return jlimit(0.0, 1.0, (double)pos / (double)jmax(1.0f, length));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoomFader)
};
