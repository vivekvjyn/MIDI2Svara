#pragma once
#include <JuceHeader.h>
#include "../dsp/NoteData.h"

class CurveDrawing
{
public:
    enum class Mode { None, ClickDraw, Freehand, Erase };

    void setMode(Mode m) { mode = m; }
    Mode getMode() const { return mode; }

    void beginDrawing(NoteData* note, double startTime, float startPitch);
    void continueDrawing(double time, float pitch);
    void endDrawing();

    bool isDrawing() const { return drawing; }
    NoteData* getTargetNote() const { return targetNote; }

    
    const std::vector<PitchPoint>& getDrawnPoints() const { return drawnPoints; }

    
    double getCursorTime() const { return cursorTime; }
    float getCursorPitch() const { return cursorPitch; }

private:
    Mode mode = Mode::None;
    NoteData* targetNote = nullptr;
    bool drawing = false;
    std::vector<PitchPoint> drawnPoints;
    double lastTime = 0.0;
    double cursorTime = 0.0;
    float cursorPitch = 0.0f;
};
