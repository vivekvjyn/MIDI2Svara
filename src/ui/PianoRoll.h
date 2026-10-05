#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"
#include "../dsp/PitchCurve.h"
#include "LookAndFeel.h"
#include "NoteComponent.h"
#include "Toolbar.h"
#include <set>

class PianoRoll : public Component
{
public:
    PianoRoll(NoteSequence& notes);
    ~PianoRoll() override;

    void paint(Graphics& g) override;
    void resized() override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;

    
    void setViewRange(double startBeat, double endBeat, double lowestNote, double highestNote);
    void setPlayheadPosition(double beat) { playheadBeat = beat; repaint(); }

    
    void setCurrentTool(::Toolbar::Tool tool);


    
    struct RecPreviewPitch {
        double relTime = 0.0;
        float offset = 0.0f;
    };
    struct RecPreviewNote {
        int noteNumber; float velocity; double startBeat; double durationBeats;
        std::vector<RecPreviewPitch> pitchCurve;
    };
    void setRecordingPreview(const std::vector<RecPreviewNote>& notes) { recordingPreviewNotes = notes; repaint(); }
    void setRecording(bool r) { isRecording = r; repaint(); }
    void setPunchZone(bool active, double start = 0.0, double end = 0.0)
    {
        punchZoneActive = active;
        punchZoneStart = start;
        punchZoneEnd = end;
        repaint();
    }

    
    void undo();
    void redo();
    void saveUndoState();

    
    bool deleteSelection();
    void selectAll();

    
    void transposeSelection(int semitones);

    
    
    
    
    
    void setRagaScale(const std::set<int>& intervals, int rootMidiNote, int vadiInterval)
    {
        ragaScale = intervals;
        ragaRootMidi = rootMidiNote;
        ragaVadiInterval = vadiInterval;
        rowCacheValid = false;
        repaint();
    }

    bool isNoteInRaga(int midiNote) const
    {
        if (ragaScale.empty()) return true;
        int rel = ((midiNote - ragaRootMidi) % 12 + 12) % 12;
        return ragaScale.count(rel) > 0;
    }

    
    double beatAtX(float x) const;
    float xForBeat(double beat) const;
    int noteAtY(float y) const;
    float yForNote(double note) const;
    float yForCents(double cents) const;
    double centsAtY(float y) const;
    float yForNoteOffset(double noteNumber, float offset) const;
    float noteOffsetAtY(double noteNumber, float y) const;

    const std::vector<int>& ragaRows() const;
    float rowHeight() const;
    float rowTopY(int note) const;
    bool overlapsOtherNote(const NoteData* self, double startBeat,
                           double endBeat, int noteNumber) const;

    
    double getViewStartBeat() const { return viewStartBeat; }
    double getViewEndBeat() const { return viewEndBeat; }
    double getViewLowest() const { return viewLowest; }
    double getViewHighest() const { return viewHighest; }

    

    
    
    void setVisibleWidthFraction(float f) { visibleWidthFraction = jlimit(0.1f, 1.0f, f); }

    
    
    
    bool updateFollow(double beat, bool isPlaying = true);

    std::function<void(NoteData*)> onNoteSelected;
    std::function<void()> onNotesChanged;
    std::function<void()> onViewChanged;

private:
    void drawGrid(Graphics& g);
    void drawGridBackground(Graphics& g);
    void drawGridLines(Graphics& g);
    
    
    
    enum class NoteDrawPass { Bodies, Curves, Both };
    void drawNotes(Graphics& g, NoteDrawPass pass = NoteDrawPass::Both);
    void drawDefaultPitchLines(Graphics& g);
    void drawPlayhead(Graphics& g);
    void drawHoverFeedback(Graphics& g);
    void drawMarqueeSelection(Graphics& g);
    void drawRecordingOverlay(Graphics& g);
    void drawRecordingPreview(Graphics& g);

    NoteData* findNoteAt(float x, float y);
    int findControlPointAt(NoteData* note, float x, float y);
    bool findAnyControlPoint(float x, float y, NoteData*& outNote, int& outIndex);
    double snapBeat(double beat) const;

    Rectangle<float> boundsForNote(const NoteData& note) const;

    NoteSequence& noteSequence;

    ::Toolbar::Tool currentTool = ::Toolbar::Tool::Pencil;

    bool snapToGrid = true;
    double gridDivision = 0.5;

    
    double viewStartBeat = 0.0;
    double viewEndBeat = 16.0;
    double viewLowest = 36.0;
    double viewHighest = 84.0;
    double playheadBeat = 0.0;

    
    bool followEnabled = true;
    float visibleWidthFraction = 1.0f;

    
    NoteData* selectedNote = nullptr;
    NoteData* hoveredNote = nullptr;
    int draggedPointIndex = -1;
    int hoveredPointIndex = -1;
    NoteData* hoveredPointNote = nullptr;

    
    enum class DragMode { None, DragPoint, MoveNote, ResizeStart, ResizeEnd, AdjustCurvature, VibratoHandle, MarqueeSelect, MarqueeMove, ScaleHorizontal, ScaleVertical, MoveLine, DrawNote };
    DragMode dragMode = DragMode::None;
    double noteDragStartBeat = 0.0;
    int noteDragStartNote = 0;
    double noteDragOrigDuration = 0.0;
    bool undoSavedForDrag = false;
    bool duplicatedForDrag = false;
    static constexpr float handleHeight = 7.0f;
    static constexpr float edgeThreshold = 8.0f;

    
    enum class HoverZone { None, Body, Handle, HandleLeftEdge, HandleRightEdge, ControlPoint };
    HoverZone currentHoverZone = HoverZone::None;
    NoteData* handleHoveredNote = nullptr;

    
    bool isNearHandle(const NoteData& note, float mx, float my, float proximity = 12.0f) const;

    
    bool findCurveSegmentAt(float mx, float my, NoteData*& outNote, int& outSegIndex);
    bool findCurveSegmentWithDistance(float mx, float my, NoteData*& outNote, int& outSegIndex, float& outDist);
    NoteData* hoveredSegmentNote = nullptr;
    int hoveredSegmentIndex = -1;
    NoteData* curvatureDragNote = nullptr;
    int curvatureDragSegIndex = -1;
    float curvatureDragStartValue = 1.0f;

    
    enum class LineHoverMode { None, Near, OnLine };
    LineHoverMode lineHoverMode = LineHoverMode::None;
    bool highlightEntireCurve = false;
    Point<float> previewDotPos;
    NoteData* lineDragNote = nullptr;
    struct LineDragOrigPos { double time; double pitchOffset; };
    std::vector<LineDragOrigPos> lineDragOrigPositions;

    

    

    
    bool deselectedOnDown = false;
    bool pointClickedOnDown = false;
    NoteData* clickedPointNote = nullptr;
    int clickedPointIndex = -1;
    Point<float> mouseDownPos;
    static constexpr float dragThreshold = 4.0f;

    
    bool rightClickErasing = false;
    bool rightClickUndoSaved = false;
    void eraseDotsNear(float mx, float my);

    
    bool cutDragging = false;
    Point<float> cutLineEnd;
    
    std::pair<NoteData, NoteData> buildSplitPair(const NoteData& src, double absoluteCutBeat) const;

    
    enum class VibratoHandle { None, Depth, Rate, FadeIn, FadeOut, Offset, Waveform };
    struct VibHandlePositions {
        Point<float> depth, rate, fadeIn, fadeOut, offset, waveform;
    };
    VibHandlePositions getVibHandlePositions(const NoteData& note, int segIdx) const;
    VibratoHandle findVibratoHandleAt(float mx, float my, NoteData*& outNote, int& outSegIdx);
    void drawVibratoHandles(Graphics& g);

    VibratoHandle vibratoHandleDragging = VibratoHandle::None;
    NoteData* vibratoDragNote = nullptr;
    int vibratoDragSegIndex = -1;
    float vibratoDragStartValue = 0.0f;

    
    VibratoHandle hoveredVibratoHandle = VibratoHandle::None;
    NoteData* hoveredVibHandleNote = nullptr;
    int hoveredVibHandleSeg = -1;

    
    struct SelectedItem {
        NoteData* note = nullptr;
        int pointIndex = -1;
    };
    std::vector<SelectedItem> multiSelection;

    Rectangle<float> marqueeRect;
    bool marqueeActive = false;

    
    void updateMarqueeSelection();
    bool isInMultiSelection(NoteData* note, int pointIdx = -1) const;
    void clearMultiSelection();

    
    Point<float> multiDragStart;
    struct OrigPos { NoteData* note; int pointIdx; double beat; double pitch; };
    std::vector<OrigPos> multiDragOrigPositions;

    
    enum class ScaleEdge { None, Left, Right, Top, Bottom };
    ScaleEdge hoveredScaleEdge = ScaleEdge::None;
    ScaleEdge activeScaleEdge = ScaleEdge::None;
    Rectangle<float> getMultiSelectionBoundingBox() const;
    void drawSelectionBoundingBox(Graphics& g);
    ScaleEdge findScaleEdgeAt(float mx, float my) const;
    float scaleDragStartMouse = 0.0f;
    float scaleDragAnchor = 0.0f;
    float scaleDragMovingEdge = 0.0f;

    
    std::vector<std::vector<NoteData>> undoStack;
    std::vector<std::vector<NoteData>> redoStack;
    static constexpr int maxUndoLevels = 50;

    
    bool isRecording = false;
    std::vector<RecPreviewNote> recordingPreviewNotes;

    
    bool punchZoneActive = false;
    double punchZoneStart = 0.0;
    double punchZoneEnd = 0.0;

private:
    

    

    
    std::set<int> ragaScale;
    int ragaRootMidi = 60;
    int ragaVadiInterval = -1;
    mutable std::vector<int> rowCache;
    mutable bool rowCacheValid = false;

    void paintHorizontalScrollbar(Graphics& g);
    void mouseDownScrollbar(const MouseEvent& e);
    void mouseDragScrollbar(const MouseEvent& e);
    void updateBeatFromScrollbar();
    bool scrollbarDragging = false;
    float scrollbarThumbX = 0.0f;
    float scrollbarThumbW = 0.0f;
    float scrollbarY = 0.0f;
    static constexpr float scrollbarHeight = 8.0f;

    double totalContentBeats = 256.0;
};
