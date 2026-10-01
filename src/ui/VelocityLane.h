#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"
#include "LookAndFeel.h"
#include "EditorToolbar.h"


class VelocityLane : public Component,
                     public SettableTooltipClient
{
public:
    enum class LaneMode
    {
        Velocity = 0,
        Dynamics = 1
        
    };

    explicit VelocityLane(NoteSequence& seq) : noteSequence(seq)
    {
        setWantsKeyboardFocus(true);
    }

    void setLaneMode(LaneMode m)
    {
        if (m == laneMode) return;
        laneMode = m;
        clearDynamicsHover();
        dynSelection.clear();
        repaint();
    }
    LaneMode getLaneMode() const { return laneMode; }

    
    void setCurrentTool(EditorToolbar::Tool t) { currentTool = t; repaint(); }

    void setViewRange(double startBeat, double endBeat)
    {
        viewStartBeat = startBeat;
        viewEndBeat = endBeat;
        repaint();
    }

    int getDesiredHeight() const { return desiredHeight; }
    void setDesiredHeight(int h) { desiredHeight = jlimit(minHeight, 300, h); }

    
    void setLeftMargin(int m) { leftMargin = m; }
    int getLeftMargin() const { return leftMargin; }

    static constexpr int minHeight = 40;

    
    
    
    
    void setPreviewNotes(std::vector<NoteData> previewIn)
    {
        previewNotes = std::move(previewIn);
        clearDynamicsHover();
        repaint();
    }
    void clearPreviewNotes() { previewNotes.clear(); repaint(); }
    bool isPreviewActive() const { return !previewNotes.empty(); }

    
    
    void simplifyDynamicsSelection();
    void smoothDynamicsSelection(float intensity);

    
    void smoothPreviewBegin();
    void smoothPreviewUpdate(float intensity);
    void smoothPreviewCommit();
    void smoothPreviewCancel();

    
    bool selectAllDynamics();
    bool deleteSelectedDynamics();

    bool hasDynamicsSelection() const { return !dynSelection.empty(); }

    
    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    bool keyPressed(const KeyPress& key) override;

    
    std::function<void(int newHeight)> onResized;
    std::function<void()> onAutoClosed;
    std::function<void()> onVelocityChanged;       
    std::function<void()> onUndoNeeded;
    std::function<void(const MouseEvent&, const MouseWheelDetails&)> onMouseWheel;
    
    
    std::function<void()> onLaneFocused;

private:
    NoteSequence& noteSequence;
    LaneMode laneMode = LaneMode::Velocity;
    EditorToolbar::Tool currentTool = EditorToolbar::Tool::Edit;

    
    std::vector<NoteData> previewNotes;

    double viewStartBeat = 0.0;
    double viewEndBeat = 16.0;
    int    desiredHeight = 100;
    int    leftMargin = 0;

    
    static constexpr int resizeGripHeight = 5;
    bool resizeDragging = false;
    int  resizeDragStartY = 0;
    int  resizeDragStartHeight = 0;
    bool isInResizeGrip(int y) const { return y < resizeGripHeight; }

    
    Rectangle<int> tabBoundsForMode(LaneMode m) const;
    LaneMode tabModeAtPoint(Point<int> p) const;

    
    float xForBeat(double beat) const
    {
        double contentW = (double)(getWidth() - leftMargin);
        double frac = (beat - viewStartBeat) / (viewEndBeat - viewStartBeat);
        return (float)leftMargin + (float)(frac * contentW);
    }
    double beatAtX(float x) const
    {
        double contentW = (double)(getWidth() - leftMargin);
        return viewStartBeat + ((double)(x - leftMargin) / contentW) * (viewEndBeat - viewStartBeat);
    }
    float valueAtY(float y, float contentTop, float contentH) const
    {
        if (contentH < 1.0f) return 0.0f;
        return jlimit(0.0f, 1.0f, 1.0f - (y - contentTop) / contentH);
    }
    float yForValue(float v, float contentTop, float contentH) const
    {
        return contentTop + contentH * (1.0f - jlimit(0.0f, 1.0f, v));
    }

    int  noteIndexAtX(float x) const;

    
    void paintVelocity(Graphics& g, Rectangle<float> contentArea);
    void setVelocityAtMouse(const MouseEvent& e);
    bool dragging = false;

    
    
    
    void paintDynamics(Graphics& g, Rectangle<float> contentArea);

    
    struct DynItem
    {
        int noteIdx;
        int pointIdx;
        bool operator==(const DynItem& o) const
        {
            return noteIdx == o.noteIdx && pointIdx == o.pointIdx;
        }
    };
    std::vector<DynItem> dynSelection;

    bool isSelected(int noteIdx, int pointIdx) const;
    void selectionAdd(int noteIdx, int pointIdx);
    void selectionRemove(int noteIdx, int pointIdx);
    void selectionToggle(int noteIdx, int pointIdx);
    void selectionClear() { dynSelection.clear(); }

    
    struct AmpHit { int noteIdx = -1; int pointIdx = -1; };
    AmpHit dynamicsPointHitTest(Point<float> p) const;
    int    noteContainingBeat(double beat) const;

    
    int  addAmplitudePoint(int noteIdx, double absoluteBeat, float value);
    int  moveAmplitudePoint(int noteIdx, int pointIdx, double absoluteBeat, float value);
    void deleteAmplitudePoint(int noteIdx, int pointIdx);
    
    
    void eraseAmplitudePointsAt(Point<float> p);

    
    Rectangle<float> selectionPixelBounds() const;

    
    enum class EdgeHandle { None, Top, Bottom, Left, Right };
    EdgeHandle edgeHandleAtPoint(Point<float> p) const;
    Rectangle<float> edgeHandleRect(EdgeHandle e) const;

    
    enum class DynDragMode
    {
        None,
        DragPoint,        
        Draw,             
        Erase,            
        MarqueeSelect,    
        MarqueeMove,      
        ScaleHorizontal,
        ScaleVertical
    };
    DynDragMode dynDragMode = DynDragMode::None;
    int   dynDragNoteIdx = -1;
    int   dynDragPointIdx = -1;

    
    
    enum class AxisLock { None, Horizontal, Vertical };
    AxisLock dragAxisLock = AxisLock::None;
    static constexpr float axisLockThresholdPx = 4.0f;

    Rectangle<float> marqueeRect;
    bool                   marqueeActive = false;

    
    EdgeHandle dragEdge = EdgeHandle::None;
    
    float lastScaleFactor = 1.0f;
    Point<float> lastCursorPx;

    
    struct DragSnap
    {
        int noteIdx;
        int pointIdx;
        double timeAtNoteStart; 
        float  value;
    };
    std::vector<DragSnap> multiDragSnap;
    Rectangle<float> multiDragOrigBounds;
    Point<float> dragStartPx;

    
    int hoverNote = -1;
    int hoverPoint = -1;
    void clearDynamicsHover() { hoverNote = -1; hoverPoint = -1; }

    
    struct CurveSnapshot
    {
        int noteIdx;
        std::vector<AmplitudePoint> orig;
    };
    bool                       smoothPreviewActive = false;
    std::vector<CurveSnapshot> smoothSnapshots;

    
    static constexpr int tabAreaHeightPx = 38;
    static constexpr int tabHeightPx = 16;
    static constexpr int tabPadX = 3;
    static constexpr int tabPadY = 1;

    
    
    
    using NoteToPoints = std::vector<std::pair<int, std::vector<int>>>;
    NoteToPoints groupSelectionByNote() const;

    
    void noteFocused() { if (onLaneFocused) onLaneFocused(); }
    void notifyChanged()
    {
        if (onVelocityChanged) onVelocityChanged();
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelocityLane)
};
