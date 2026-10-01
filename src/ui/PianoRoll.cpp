#include "PianoRoll.h"
#include "../dsp/CarnaticEngine.h"
#include <cmath>


PianoRoll::PianoRoll(NoteSequence& notes) : noteSequence(notes)
{
    setWantsKeyboardFocus(true);
    startTimerHz(30);
}

PianoRoll::~PianoRoll() = default;

void PianoRoll::timerCallback()
{
    if (curveDrawer.isDrawing())
        repaint();
}

void PianoRoll::setViewRange(double startBeat, double endBeat, double lowest, double highest)
{
    viewStartBeat = startBeat;
    viewEndBeat = endBeat;
    viewLowest = lowest;
    viewHighest = highest;
    repaint();
}

bool PianoRoll::updateFollow(double beat, bool )
{
    if (!followEnabled) return false;

    double visibleBeats = viewEndBeat - viewStartBeat;

    double targetX = viewStartBeat + visibleBeats * 0.25;
    double diff = beat - targetX;

    if (diff > 0.0)
    {
        viewStartBeat += diff;
        viewEndBeat += diff;
        repaint();
        return true;
    }

    if (beat < viewStartBeat)
    {
        double newStart = beat - visibleBeats * 0.1;
        viewStartBeat = newStart;
        viewEndBeat = newStart + visibleBeats;
        repaint();
        return true;
    }

    return false;
}

void PianoRoll::setCurrentTool(EditorToolbar::Tool tool)
{
    currentTool = tool;
    if (tool == EditorToolbar::Tool::Pencil)
        curveDrawer.setMode(CurveDrawing::Mode::Freehand);
    else if (tool == EditorToolbar::Tool::Edit)
        curveDrawer.setMode(CurveDrawing::Mode::ClickDraw);
    else
        curveDrawer.setMode(CurveDrawing::Mode::None);
    repaint();
}

void PianoRoll::saveUndoState()
{
    undoStack.push_back(noteSequence.getAllNotes());
    if ((int)undoStack.size() > maxUndoLevels)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void PianoRoll::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back(noteSequence.getAllNotes());
    noteSequence.getAllNotes() = undoStack.back();
    undoStack.pop_back();
    selectedNote = nullptr;
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back(noteSequence.getAllNotes());
    noteSequence.getAllNotes() = redoStack.back();
    redoStack.pop_back();
    selectedNote = nullptr;
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::setGamakaStamp(const String& name, float intensity)
{
    gamakaStampName = name;
    gamakaStampIntensity = intensity;

    if (name.isNotEmpty())
    {
        auto points = PitchCurveInterpolator::generateGamaka(name, 0.0, 1.0, 1.0f);
        if (!points.empty())
        {
            const int curW = 40, curH = 28;
            Image curImg(Image::ARGB, curW, curH, true);
            Graphics g(curImg);

            float minP = 0.0f, maxP = 0.0f;
            for (auto& p : points)
            {
                minP = std::min(minP, (float)p.pitchOffset);
                maxP = std::max(maxP, (float)p.pitchOffset);
            }
            float range = std::max(maxP - minP, 0.5f);
            float margin = range * 0.1f;
            minP -= margin; maxP += margin;
            range = maxP - minP;

            g.setColour(FPColours::background.withAlpha(0.85f));
            g.fillRoundedRectangle(0.0f, 0.0f, (float)curW, (float)curH, 4.0f);
            g.setColour(FPColours::gamaka.withAlpha(0.5f));
            g.drawRoundedRectangle(0.5f, 0.5f, (float)curW - 1.0f, (float)curH - 1.0f, 4.0f, 1.0f);

            float zeroY = (float)curH - ((-minP) / range) * (float)curH;
            g.setColour(FPColours::gridLine.withAlpha(0.4f));
            g.drawHorizontalLine((int)zeroY, 3.0f, (float)curW - 3.0f);

            Path path;
            int steps = curW * 2;
            float padX = 3.0f, padY = 3.0f;
            float drawW = (float)curW - padX * 2.0f;
            float drawH = (float)curH - padY * 2.0f;
            for (int s = 0; s <= steps; ++s)
            {
                float t = (float)s / (float)steps;
                float pitch = PitchCurveInterpolator::interpolate(points, (double)t, false);
                float x = padX + t * drawW;
                float y = padY + drawH - ((pitch - minP) / range) * drawH;
                if (s == 0) path.startNewSubPath(x, y);
                else path.lineTo(x, y);
            }
            g.setColour(FPColours::gamaka);
            g.strokePath(path, PathStrokeType(1.8f));

            expressionCursor = MouseCursor(curImg, 0, 0);
        }
    }
    else
    {
        expressionCursor = MouseCursor::NormalCursor;
    }
    repaint();
}

std::vector<PitchPoint> PianoRoll::generateGamakaForNote(
    const String& name, NoteData* note, float intensity) const
{
    double dur = note ? note->durationBeats : 1.0;
    return PitchCurveInterpolator::generateGamaka(name, 0.0, dur, intensity);
}

void PianoRoll::clearExpressionStamp()
{
    gamakaStampName = {};
    gamakaStampIntensity = 1.0f;
    expressionCursor = MouseCursor::NormalCursor;
    repaint();
}

bool PianoRoll::isInMultiSelection(NoteData* note, int pointIdx) const
{
    for (auto& sel : multiSelection)
        if (sel.note == note && sel.pointIndex == pointIdx)
            return true;
    return false;
}

void PianoRoll::clearMultiSelection()
{
    multiSelection.clear();
    marqueeActive = false;
}

void PianoRoll::selectAll()
{
    clearMultiSelection();
    auto& allNotes = noteSequence.getAllNotes();
    for (auto& note : allNotes)
    {
        
        multiSelection.push_back({ &note, -1 });

        for (int i = 0; i < (int) note.pitchCurve.size(); ++i)
            multiSelection.push_back({ &note, i });
    }

    if (!allNotes.empty())
        selectedNote = &allNotes[0];

    repaint();
}

void PianoRoll::quantizeNotes()
{
    if (gridDivision <= 0.0) return;

    std::vector<NoteData*> targets;
    if (!multiSelection.empty())
    {
        for (const auto& sel : multiSelection)
        {
            if (sel.pointIndex == -1 && sel.note != nullptr)
            {
                if (std::find(targets.begin(), targets.end(), sel.note) == targets.end())
                    targets.push_back(sel.note);
            }
        }
    }
    if (targets.empty())
    {
        auto& allNotes = noteSequence.getAllNotes();
        for (auto& note : allNotes)
            targets.push_back(&note);
    }
    if (targets.empty()) return;

    saveUndoState();
    for (auto* n : targets)
    {
        double snapped = std::round(n->startBeat / gridDivision) * gridDivision;
        n->startBeat = jmax(0.0, snapped);
    }
    if (onNotesChanged) onNotesChanged();
    repaint();
}

bool PianoRoll::deleteSelection()
{
    
    if (!multiSelection.empty())
    {
        saveUndoState();

        std::set<int> noteIndicesToDelete;
        std::map<NoteData*, std::vector<int>> dotsToDelete;

        auto& allNotes = noteSequence.getAllNotes();

        for (auto& sel : multiSelection)
        {
            if (sel.pointIndex < 0)
            {
                
                for (int i = 0; i < (int)allNotes.size(); ++i)
                {
                    if (&allNotes[(size_t)i] == sel.note)
                    {
                        noteIndicesToDelete.insert(i);
                        break;
                    }
                }
            }
            else
            {
                dotsToDelete[sel.note].push_back(sel.pointIndex);
            }
        }

        for (auto& [note, indices] : dotsToDelete)
        {
            
            bool noteBeingDeleted = false;
            for (int i = 0; i < (int)allNotes.size(); ++i)
            {
                if (&allNotes[(size_t)i] == note && noteIndicesToDelete.count(i))
                {
                    noteBeingDeleted = true;
                    break;
                }
            }
            if (noteBeingDeleted) continue;

            std::sort(indices.begin(), indices.end(), std::greater<int>());
            for (int idx : indices)
            {
                if (idx < (int)note->pitchCurve.size())
                    note->pitchCurve.erase(note->pitchCurve.begin() + idx);
            }
        }

        std::vector<int> sortedIndices(noteIndicesToDelete.begin(), noteIndicesToDelete.end());
        std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
        for (int idx : sortedIndices)
        {
            if (&allNotes[(size_t)idx] == selectedNote)
                selectedNote = nullptr;
            noteSequence.removeNote(idx);
        }

        clearMultiSelection();
        if (onNotesChanged) onNotesChanged();
        repaint();
        return true;
    }

    if (selectedNote != nullptr)
    {
        saveUndoState();
        auto& notes = noteSequence.getAllNotes();
        for (int i = 0; i < (int)notes.size(); ++i)
        {
            if (&notes[(size_t)i] == selectedNote)
            {
                noteSequence.removeNote(i);
                break;
            }
        }
        selectedNote = nullptr;
        if (onNotesChanged) onNotesChanged();
        if (onNoteSelected) onNoteSelected(nullptr);
        repaint();
        return true;
    }

    return false;
}

void PianoRoll::transposeSelection(int semitones)
{
    if (semitones == 0) return;

    std::set<NoteData*> notesToTranspose;

    if (!multiSelection.empty())
    {
        for (auto& sel : multiSelection)
        {
            if (sel.pointIndex < 0 && sel.note != nullptr)
                notesToTranspose.insert(sel.note);
        }
    }

    if (notesToTranspose.empty() && selectedNote != nullptr)
        notesToTranspose.insert(selectedNote);

    if (notesToTranspose.empty()) return;

    saveUndoState();
    for (auto* note : notesToTranspose)
    {
        int newNote = jlimit(0, 127, note->noteNumber + semitones);
        note->noteNumber = newNote;
    }

    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::simplifySelection()
{
    if (multiSelection.empty()) return;

    std::map<NoteData*, std::vector<int>> noteIndices;
    for (auto& sel : multiSelection)
    {
        if (sel.pointIndex >= 0)
            noteIndices[sel.note].push_back(sel.pointIndex);
    }

    if (noteIndices.empty()) return;
    saveUndoState();

    for (auto& [note, indices] : noteIndices)
    {
        if (indices.size() < 3) continue;
        std::sort(indices.begin(), indices.end());

        int startIdx = indices.front();
        int endIdx = indices.back();

        PitchCurveInterpolator::simplifyRange(note->pitchCurve, startIdx, endIdx, 0.05f);
    }

    clearMultiSelection();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::smoothSelection()
{
    smoothSelection(0.6f);
}

void PianoRoll::smoothSelection(float intensity)
{
    if (multiSelection.empty()) return;

    std::map<NoteData*, std::vector<int>> noteIndices;
    for (auto& sel : multiSelection)
    {
        if (sel.pointIndex >= 0)
            noteIndices[sel.note].push_back(sel.pointIndex);
    }

    if (noteIndices.empty()) return;
    saveUndoState();

    for (auto& [note, indices] : noteIndices)
    {
        if (indices.size() < 3) continue;
        std::sort(indices.begin(), indices.end());
        PitchCurveInterpolator::smoothRange(note->pitchCurve, indices.front(), indices.back(), intensity);
    }

    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::smoothPreviewBegin()
{
    if (multiSelection.empty()) return;
    smoothPreviewActive = true;
    smoothSnapshots.clear();

    std::set<NoteData*> seen;
    for (auto& sel : multiSelection)
    {
        if (sel.pointIndex >= 0 && seen.insert(sel.note).second)
            smoothSnapshots.push_back({ sel.note, sel.note->pitchCurve });
    }

    saveUndoState();
}

void PianoRoll::smoothPreviewUpdate(float intensity)
{
    if (!smoothPreviewActive || smoothSnapshots.empty()) return;

    std::map<NoteData*, std::vector<int>> noteIndices;
    for (auto& sel : multiSelection)
    {
        if (sel.pointIndex >= 0)
            noteIndices[sel.note].push_back(sel.pointIndex);
    }

    for (auto& snap : smoothSnapshots)
    {
        snap.note->pitchCurve = snap.originalCurve;

        auto it = noteIndices.find(snap.note);
        if (it != noteIndices.end() && it->second.size() >= 3)
        {
            auto& indices = it->second;
            std::sort(indices.begin(), indices.end());
            PitchCurveInterpolator::smoothRange(snap.note->pitchCurve,
                                                 indices.front(), indices.back(), intensity);
        }
    }

    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::smoothPreviewCommit()
{
    
    smoothPreviewActive = false;
    smoothSnapshots.clear();
}

void PianoRoll::smoothPreviewCancel()
{
    if (!smoothPreviewActive) return;

    for (auto& snap : smoothSnapshots)
        snap.note->pitchCurve = snap.originalCurve;

    smoothPreviewActive = false;
    smoothSnapshots.clear();

    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::mergeSelectedNotes()
{
    
    std::vector<NoteData*> targets;
    std::set<NoteData*> seen;
    for (auto& sel : multiSelection)
    {
        if (sel.note && sel.pointIndex < 0 && seen.insert(sel.note).second)
            targets.push_back(sel.note);
    }
    if (targets.size() < 2) return;

    saveUndoState();

    std::sort(targets.begin(), targets.end(),
        [](NoteData* a, NoteData* b) { return a->startBeat < b->startBeat; });

    double earliest = targets.front()->startBeat;
    double latest = targets.front()->getEndBeat();
    int lowestNote = targets.front()->noteNumber;

    for (auto* n : targets)
    {
        latest = std::max(latest, n->getEndBeat());
        lowestNote = std::min(lowestNote, n->noteNumber);
    }

    std::vector<PitchPoint> mergedCurve;
    for (auto* n : targets)
    {
        float semitoneOffset = (float)(n->noteNumber - lowestNote);

        if (n->pitchCurve.empty())
        {
            
            PitchPoint p;
            p.time = n->startBeat - earliest;
            p.pitchOffset = (double)semitoneOffset;
            p.curveType = PitchPoint::CurveType::Linear;
            mergedCurve.push_back(p);

            p.time = n->getEndBeat() - earliest;
            mergedCurve.push_back(p);
        }
        else
        {
            for (auto& pt : n->pitchCurve)
            {
                PitchPoint mp;
                mp.time = (n->startBeat - earliest) + pt.time;
                mp.pitchOffset = pt.pitchOffset + (double)semitoneOffset;
                mp.curveType = pt.curveType;
                mp.curvature = pt.curvature;
                mp.bias = pt.bias;
                mergedCurve.push_back(mp);
            }
        }
    }

    std::sort(mergedCurve.begin(), mergedCurve.end(),
        [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });

    NoteData* keeper = targets[0];
    for (size_t i = 1; i < targets.size(); ++i)
    {
        for (int j = 0; j < noteSequence.getNumNotes(); ++j)
        {
            if (&noteSequence.getNote(j) == targets[i])
            {
                noteSequence.removeNote(j);
                break;
            }
        }
    }

    keeper->noteNumber = lowestNote;
    keeper->startBeat = earliest;
    keeper->durationBeats = latest - earliest;
    keeper->pitchCurve = std::move(mergedCurve);

    multiSelection.clear();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

double PianoRoll::beatAtX(float x) const
{
    double range = viewEndBeat - viewStartBeat;
    return viewStartBeat + (double)x / (double)getWidth() * range;
}

float PianoRoll::xForBeat(double beat) const
{
    double range = viewEndBeat - viewStartBeat;
    if (range <= 0.0) return 0.0f;
    return (float)((beat - viewStartBeat) / range * getWidth());
}

int PianoRoll::noteAtY(float y) const
{
    double noteRange = viewHighest - viewLowest;
    if (noteRange <= 0.0) return (int)viewLowest;
    return (int)(viewHighest - (double)y / (double)getHeight() * noteRange);
}

float PianoRoll::yForNote(double note) const
{
    double noteRange = viewHighest - viewLowest;
    if (noteRange <= 0.0) return 0.0f;
    return (float)((viewHighest - note) / noteRange * (double)getHeight());
}

float PianoRoll::getPixelsPerSemitone() const
{
    double noteRange = viewHighest - viewLowest;
    if (noteRange <= 0.0) return 16.0f;
    return (float)((double)getHeight() / noteRange);
}

Rectangle<float> PianoRoll::boundsForNote(const NoteData& note) const
{
    float x = xForBeat(note.startBeat);
    float w = xForBeat(note.getEndBeat()) - x;
    float y = yForNote(note.noteNumber + 1);
    float h = getPixelsPerSemitone();
    return { x, y, jmax(4.0f, w), jmax(4.0f, h) };
}

float PianoRoll::snapPitch(float pitch) const
{
    if (snapMode == EditorToolbar::SnapMode::Grid)
        return std::round(pitch);
    return pitch;
}

double PianoRoll::snapBeat(double beat) const
{
    if (snapMode == EditorToolbar::SnapMode::Grid && gridDivision > 0.0)
        return std::round(beat / gridDivision) * gridDivision;
    return beat;
}

NoteData* PianoRoll::findNoteAt(float x, float y)
{
    for (auto& n : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(n);
        if (bounds.contains(x, y))
            return &n;
    }
    return nullptr;
}

int PianoRoll::findControlPointAt(NoteData* note, float x, float y)
{
    if (note == nullptr) return -1;

    auto bounds = boundsForNote(*note);
    float pps = getPixelsPerSemitone();
    float hitRadius = 8.0f;

    for (int i = 0; i < (int)note->pitchCurve.size(); ++i)
    {
        auto& pt = note->pitchCurve[(size_t)i];
        float px = bounds.getX() + (float)(pt.time / note->durationBeats) * bounds.getWidth();
        float py = bounds.getCentreY() - (float)pt.pitchOffset * pps;
        float dx = x - px;
        float dy = y - py;
        if (dx * dx + dy * dy < hitRadius * hitRadius)
            return i;
    }
    return -1;
}

bool PianoRoll::findAnyControlPoint(float x, float y, NoteData*& outNote, int& outIndex)
{
    float pps = getPixelsPerSemitone();
    float hitRadius = 8.0f;
    float bestDist = hitRadius * hitRadius;

    outNote = nullptr;
    outIndex = -1;

    for (auto& note : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(note);

        if (x < bounds.getX() - 10 || x > bounds.getRight() + 10)
            continue;

        for (int i = 0; i < (int)note.pitchCurve.size(); ++i)
        {
            auto& pt = note.pitchCurve[(size_t)i];
            float px = bounds.getX() + (float)(pt.time / note.durationBeats) * bounds.getWidth();
            float py = bounds.getCentreY() - (float)pt.pitchOffset * pps;
            float dx = x - px;
            float dy = y - py;
            float dist = dx * dx + dy * dy;
            if (dist < bestDist)
            {
                bestDist = dist;
                outNote = &note;
                outIndex = i;
            }
        }
    }

    return outNote != nullptr;
}

bool PianoRoll::findCurveSegmentWithDistance(float mx, float my, NoteData*& outNote, int& outSegIndex, float& outDist)
{
    float hitDist = 32.0f;  
    float bestDist = hitDist;
    outNote = nullptr;
    outSegIndex = -1;
    outDist = hitDist;

    for (auto& note : noteSequence.getAllNotes())
    {
        if (note.pitchCurve.size() < 2) continue;

        auto bounds = boundsForNote(note);
        if (bounds.getRight() < 0 || bounds.getX() > getWidth()) continue;
        if (mx < bounds.getX() - 10 || mx > bounds.getRight() + 10) continue;

        float pps = getPixelsPerSemitone();
        float noteW = bounds.getWidth();
        float dur = (float)note.durationBeats;

        for (int seg = 0; seg < (int)note.pitchCurve.size() - 1; ++seg)
        {
            auto& ptA = note.pitchCurve[(size_t)seg];
            auto& ptB = note.pitchCurve[(size_t)seg + 1];

            float xA = bounds.getX() + (float)(ptA.time / dur) * noteW;
            float xB = bounds.getX() + (float)(ptB.time / dur) * noteW;

            if (mx < xA - 5 || mx > xB + 5) continue;

            double t = (double)(mx - xA) / (double)(xB - xA);
            t = jlimit(0.0, 1.0, t);
            double time = ptA.time + t * (ptB.time - ptA.time);
            float pitchAtMouse = PitchCurveInterpolator::interpolate(note.pitchCurve, time);

            float curveY = bounds.getCentreY() - pitchAtMouse * pps;
            float dist = std::abs(my - curveY);

            if (dist < bestDist)
            {
                bestDist = dist;
                outNote = &note;
                outSegIndex = seg;
            }
        }
    }
    outDist = bestDist;
    return outNote != nullptr;
}

bool PianoRoll::findCurveSegmentAt(float mx, float my, NoteData*& outNote, int& outSegIndex)
{
    float dist = 0.0f;
    bool found = findCurveSegmentWithDistance(mx, my, outNote, outSegIndex, dist);
    return found && dist < 28.0f;  
}

bool PianoRoll::isInHandle(const NoteData& note, float mx, float my) const
{
    auto bounds = boundsForNote(note);
    float handleTop = bounds.getY();
    float handleBottom = bounds.getY() + handleHeight;
    return mx >= bounds.getX() - 2 && mx <= bounds.getRight() + 2
        && my >= handleTop - 2 && my <= handleBottom;
}

bool PianoRoll::isNearHandle(const NoteData& note, float mx, float my, float proximity) const
{
    auto bounds = boundsForNote(note);
    float handleTop = bounds.getY();
    float handleBottom = bounds.getY() + handleHeight;
    return mx >= bounds.getX() - 2 && mx <= bounds.getRight() + 2
        && my >= handleTop - proximity && my <= handleBottom + proximity;
}

PianoRoll::VibHandlePositions PianoRoll::getVibHandlePositions(const NoteData& note, int segIdx) const
{
    auto& ptA = note.pitchCurve[(size_t)segIdx];
    auto& ptB = note.pitchCurve[(size_t)segIdx + 1];
    auto& vib = ptA.vibrato;

    auto bounds = boundsForNote(note);
    float pps = getPixelsPerSemitone();
    float dur = (float)note.durationBeats;
    float noteW = bounds.getWidth();
    float xA = bounds.getX() + (float)(ptA.time / dur) * noteW;
    float xB = bounds.getX() + (float)(ptB.time / dur) * noteW;

    auto posAt = [&](float frac) -> Point<float> {
        float x = xA + frac * (xB - xA);
        double time = ptA.time + frac * (ptB.time - ptA.time);
        float basePitch = PitchCurveInterpolator::interpolate(note.pitchCurve, time, false);
        float y = bounds.getCentreY() - basePitch * pps;
        return { x, y };
    };

    VibHandlePositions h;

    auto depthPos = posAt(0.18f);
    h.depth = { depthPos.x, depthPos.y - vib.depth * pps };

    h.rate = posAt(0.5f);

    h.fadeIn = posAt(jmax(0.02f, vib.fadeInFrac));

    h.fadeOut = posAt(jmin(0.98f, 1.0f - vib.fadeOutFrac));

    auto offsetPos = posAt(0.75f);
    h.offset = { offsetPos.x, offsetPos.y - vib.offset * pps };

    h.waveform = { h.rate.x, h.rate.y + 18.0f };

    return h;
}

PianoRoll::VibratoHandle PianoRoll::findVibratoHandleAt(float mx, float my,
                                                             NoteData*& outNote, int& outSegIdx)
{
    float hitR2 = 12.0f * 12.0f; 

    for (auto& note : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(note);
        if (bounds.getRight() < 0 || bounds.getX() > getWidth()) continue;
        if (mx < bounds.getX() - 20 || mx > bounds.getRight() + 20) continue;

        for (int seg = 0; seg < (int)note.pitchCurve.size() - 1; ++seg)
        {
            if (!note.pitchCurve[(size_t)seg].vibrato.enabled) continue;

            auto h = getVibHandlePositions(note, seg);

            {
                float wfW = 36.0f, wfH = 14.0f;
                if (mx >= h.waveform.x - wfW * 0.5f && mx <= h.waveform.x + wfW * 0.5f
                    && my >= h.waveform.y - wfH * 0.5f && my <= h.waveform.y + wfH * 0.5f)
                {
                    outNote = &note;
                    outSegIdx = seg;
                    return VibratoHandle::Waveform;
                }
            }

            struct HandleCheck { VibratoHandle type; Point<float> pos; };
            HandleCheck checks[] = {
                { VibratoHandle::Depth,   h.depth   },
                { VibratoHandle::Rate,    h.rate    },
                { VibratoHandle::FadeIn,  h.fadeIn  },
                { VibratoHandle::FadeOut, h.fadeOut  },
                { VibratoHandle::Offset,  h.offset  }
            };

            for (auto& ck : checks)
            {
                float dx = mx - ck.pos.x;
                float dy = my - ck.pos.y;
                if (dx * dx + dy * dy < hitR2)
                {
                    outNote = &note;
                    outSegIdx = seg;
                    return ck.type;
                }
            }
        }
    }

    outNote = nullptr;
    outSegIdx = -1;
    return VibratoHandle::None;
}

void PianoRoll::drawVibratoHandles(Graphics& g)
{
    for (auto& note : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(note);
        if (bounds.getRight() < 0 || bounds.getX() > getWidth()) continue;

        float pps = getPixelsPerSemitone();
        float dur = (float)note.durationBeats;
        float noteW = bounds.getWidth();

        for (int seg = 0; seg < (int)note.pitchCurve.size() - 1; ++seg)
        {
            auto& ptA = note.pitchCurve[(size_t)seg];
            if (!ptA.vibrato.enabled) continue;

            auto& ptB = note.pitchCurve[(size_t)seg + 1];
            auto& vib = ptA.vibrato;

            float xA = bounds.getX() + (float)(ptA.time / dur) * noteW;
            float xB = bounds.getX() + (float)(ptB.time / dur) * noteW;
            float segW = xB - xA;
            float segDur = (float)(ptB.time - ptA.time);

            Path envTop, envBottom;
            int envSteps = jmax(20, (int)(segW * 0.5f));
            for (int s = 0; s <= envSteps; ++s)
            {
                float frac = (float)s / (float)envSteps;
                float x = xA + frac * segW;
                double time = ptA.time + frac * (ptB.time - ptA.time);
                float basePitch = PitchCurveInterpolator::interpolate(note.pitchCurve, time, false);

                float localTime = frac * segDur;
                float envelope = 1.0f;
                float fadeInTime = vib.fadeInFrac * segDur;
                float fadeOutTime = vib.fadeOutFrac * segDur;
                if (fadeInTime > 0.001f && localTime < fadeInTime)
                    envelope *= localTime / fadeInTime;
                float timeFromEnd = segDur - localTime;
                if (fadeOutTime > 0.001f && timeFromEnd < fadeOutTime)
                    envelope *= timeFromEnd / fadeOutTime;

                float amp = vib.depth * envelope;
                float ofsEnv = vib.offset * envelope;
                float topY = bounds.getCentreY() - (basePitch + ofsEnv + amp) * pps;
                float botY = bounds.getCentreY() - (basePitch + ofsEnv - amp) * pps;

                if (s == 0)
                {
                    envTop.startNewSubPath(x, topY);
                    envBottom.startNewSubPath(x, botY);
                }
                else
                {
                    envTop.lineTo(x, topY);
                    envBottom.lineTo(x, botY);
                }
            }

            g.setColour(FPColours::vibrato.withAlpha(0.18f));
            g.strokePath(envTop, PathStrokeType(1.0f));
            g.strokePath(envBottom, PathStrokeType(1.0f));

            g.setColour(FPColours::vibrato.withAlpha(0.22f));
            if (vib.fadeInFrac > 0.01f)
            {
                float fadeInX = xA + vib.fadeInFrac * segW;
                float baseFI = PitchCurveInterpolator::interpolate(note.pitchCurve,
                    ptA.time + vib.fadeInFrac * (ptB.time - ptA.time), false);
                float yFI = bounds.getCentreY() - baseFI * pps;
                g.drawLine(fadeInX, yFI - vib.depth * pps - 5, fadeInX, yFI + vib.depth * pps + 5, 1.0f);
            }
            if (vib.fadeOutFrac > 0.01f)
            {
                float fadeOutX = xB - vib.fadeOutFrac * segW;
                float baseFO = PitchCurveInterpolator::interpolate(note.pitchCurve,
                    ptA.time + (1.0f - vib.fadeOutFrac) * (ptB.time - ptA.time), false);
                float yFO = bounds.getCentreY() - baseFO * pps;
                g.drawLine(fadeOutX, yFO - vib.depth * pps - 5, fadeOutX, yFO + vib.depth * pps + 5, 1.0f);
            }

            auto h = getVibHandlePositions(note, seg);
            bool isThisSeg = (&note == hoveredVibHandleNote && seg == hoveredVibHandleSeg);

            auto drawHandle = [&](Point<float> pos, Colour col,
                                  VibratoHandle type, bool diamond = false) {
                bool isHov = isThisSeg && hoveredVibratoHandle == type;
                float r = isHov ? 5.5f : 4.0f;

                if (isHov)
                {
                    g.setColour(col.withAlpha(0.3f));
                    g.fillEllipse(pos.x - r - 2, pos.y - r - 2, (r + 2) * 2, (r + 2) * 2);
                }

                if (diamond)
                {
                    Path d;
                    d.addTriangle(pos.x, pos.y - r, pos.x + r, pos.y, pos.x, pos.y + r);
                    d.addTriangle(pos.x, pos.y - r, pos.x - r, pos.y, pos.x, pos.y + r);
                    g.setColour(col);
                    g.fillPath(d);
                    g.setColour(Colours::white.withAlpha(0.5f));
                    g.strokePath(d, PathStrokeType(1.0f));
                }
                else
                {
                    g.setColour(col);
                    g.fillEllipse(pos.x - r, pos.y - r, r * 2, r * 2);
                    g.setColour(Colours::white.withAlpha(0.5f));
                    g.drawEllipse(pos.x - r, pos.y - r, r * 2, r * 2, 1.0f);
                }
            };

            drawHandle(h.depth,   Colour(0xff00ccdd), VibratoHandle::Depth, true);   
            drawHandle(h.rate,    Colour(0xff44bb44), VibratoHandle::Rate);           
            drawHandle(h.fadeIn,  Colour(0xffccaa44), VibratoHandle::FadeIn);         
            drawHandle(h.fadeOut, Colour(0xffccaa44), VibratoHandle::FadeOut);        
            drawHandle(h.offset,  Colour(0xffdd8844), VibratoHandle::Offset);         

            {
                const char* wfName = vibratoWaveformName(vib.waveform);
                float wfW = 36.0f, wfH = 14.0f;
                bool wfHov = isThisSeg && hoveredVibratoHandle == VibratoHandle::Waveform;
                auto wfCol = wfHov ? FPColours::text : FPColours::textDim;
                auto wfBg = wfHov ? FPColours::surfaceLight : FPColours::surface;

                g.setColour(wfBg.withAlpha(0.9f));
                g.fillRoundedRectangle(h.waveform.x - wfW * 0.5f, h.waveform.y - wfH * 0.5f,
                                       wfW, wfH, 3.0f);
                g.setColour(FPColours::vibrato.withAlpha(0.5f));
                g.drawRoundedRectangle(h.waveform.x - wfW * 0.5f, h.waveform.y - wfH * 0.5f,
                                       wfW, wfH, 3.0f, 1.0f);
                g.setColour(wfCol);
                g.setFont(FontOptions(9.0f));
                g.drawText(wfName, (int)(h.waveform.x - wfW * 0.5f), (int)(h.waveform.y - wfH * 0.5f),
                           (int)wfW, (int)wfH, Justification::centred);
            }

            if (isThisSeg && hoveredVibratoHandle != VibratoHandle::None)
            {
                String label;
                Point<float> pos;
                switch (hoveredVibratoHandle)
                {
                    case VibratoHandle::Depth:    label = "Depth: " + String(vib.depth, 2) + " st";  pos = h.depth;   break;
                    case VibratoHandle::Rate:     label = "Rate: " + String(vib.rate, 1) + "/beat";   pos = h.rate;    break;
                    case VibratoHandle::FadeIn:   label = "Fade In: " + String((int)(vib.fadeInFrac * 100)) + "%";   pos = h.fadeIn;  break;
                    case VibratoHandle::FadeOut:  label = "Fade Out: " + String((int)(vib.fadeOutFrac * 100)) + "%"; pos = h.fadeOut; break;
                    case VibratoHandle::Offset:   label = "Offset: " + String(vib.offset, 2) + " st"; pos = h.offset; break;
                    case VibratoHandle::Waveform: break;
                    case VibratoHandle::None:     break;
                }
                if (label.isNotEmpty())
                {
                    float labelW = 100.0f;
                    float lx = pos.x + 12;
                    
                    if (lx + labelW > getWidth()) lx = pos.x - labelW - 8;
                    g.setColour(FPColours::surface.withAlpha(0.92f));
                    g.fillRoundedRectangle(lx, pos.y - 10, labelW, 18, 3);
                    g.setColour(FPColours::text);
                    g.setFont(FontOptions(10.0f));
                    g.drawText(label, (int)lx + 4, (int)(pos.y - 10), (int)labelW - 8, 18,
                               Justification::centredLeft);
                }
            }
        }
    }
}

void PianoRoll::drawGrid(Graphics& g)
{
    drawGridBackground(g, false);
    drawGridLines(g, false);
}

void PianoRoll::drawGridBackground(Graphics& g, bool specVisible)
{
    float w = (float)getWidth();

    int lowInt = (int)std::floor(viewLowest);
    int highInt = (int)std::ceil(viewHighest);

    float bgAlpha = specVisible ? 0.25f : 1.0f;

    for (int note = lowInt; note <= highInt; ++note)
    {
        float rowTop = yForNote((double)(note + 1));
        float rowBot = yForNote((double)note);
        float rowH = rowBot - rowTop;

        int n = note % 12;
        bool black = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);

        auto rowColour = black ? Colour(0xff1e1e2c) : Colour(0xff2c2c40);
        g.setColour(rowColour.withAlpha(bgAlpha));
        g.fillRect(0.0f, rowTop, w, rowH);
    }
}

void PianoRoll::drawGridLines(Graphics& g, bool specVisible)
{
    float w = (float)getWidth();
    float h = (float)getHeight();

    float lineAlpha = specVisible ? 0.4f : 1.0f;

    int lowInt = (int)std::floor(viewLowest);
    int highInt = (int)std::ceil(viewHighest);

    for (int note = lowInt; note <= highInt; ++note)
    {
        float y = yForNote((double)note);
        bool isC = (note % 12 == 0);
        auto col = isC ? Colour(0xff4a4a60) : Colour(0xff38384c);
        g.setColour(col.withAlpha(lineAlpha));
        g.drawHorizontalLine((int)y, 0.0f, w);
    }

    int firstBeat = (int)std::floor(viewStartBeat);
    int lastBeat = (int)std::ceil(viewEndBeat);
    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        float x = xForBeat((double)beat);
        bool isBarLine = (beat % 4 == 0);
        auto col = isBarLine ? Colour(0xff4a4a60) : Colour(0xff38384c);
        g.setColour(col.withAlpha(lineAlpha));
        g.drawVerticalLine((int)x, 0.0f, h);

        for (int sub = 1; sub < 4; ++sub)
        {
            float subX = xForBeat((double)beat + (double)sub / 4.0);
            g.setColour(Colour(0xff30303e).withAlpha(lineAlpha));
            g.drawVerticalLine((int)subX, 0.0f, h);
        }
    }
}

void PianoRoll::drawDefaultPitchLines(Graphics& g)
{
    
    auto& notes = noteSequence.getAllNotes();

    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& note = notes[i];
        auto bounds = boundsForNote(note);

        if (bounds.getRight() < 0 || bounds.getX() > getWidth())
            continue;

        if (note.pitchCurve.empty())
        {
            g.setColour(FPColours::pitchCurve.withAlpha(0.6f));
            float cy = bounds.getCentreY();
            g.drawLine(bounds.getX(), cy, bounds.getRight(), cy, 1.5f);
        }
    }
}

void PianoRoll::drawNotes(Graphics& g, NoteDrawPass pass)
{
    const bool drawBodies = (pass == NoteDrawPass::Bodies || pass == NoteDrawPass::Both);
    const bool drawCurves = (pass == NoteDrawPass::Curves || pass == NoteDrawPass::Both);

    float pps = getPixelsPerSemitone();
    float noteAlpha = 1.0f;

    for (auto& note : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(note);
        if (bounds.getRight() < 0 || bounds.getX() > getWidth())
            continue;

        double noteEnd = note.startBeat + note.durationBeats;
        bool inPunchZone = punchZoneActive
                           && noteEnd > punchZoneStart
                           && note.startBeat < punchZoneEnd;

        if (inPunchZone)
            continue;

        bool isSelected = (&note == selectedNote);

        float perNoteAlpha = noteAlpha;

        if (drawBodies)
            NoteComponent::drawNote(g, note, bounds, isSelected, perNoteAlpha);

        if (drawCurves)
        {
            int segHover = -1;
            if (&note == hoveredSegmentNote)
            {
                if (highlightEntireCurve)
                    segHover = -2;  
                else
                    segHover = hoveredSegmentIndex;
            }
            NoteComponent::drawPitchCurve(g, note, bounds, pps, segHover);
        }

        if (drawBodies)
        {
            bool showHandle = (&note == handleHoveredNote) || isSelected;
            if (showHandle)
            {
                auto handleRect = Rectangle<float>(
                    bounds.getX(), bounds.getY(), bounds.getWidth(), handleHeight);

                g.setColour(FPColours::pitchCurveAlt.withAlpha(0.45f));
                g.fillRoundedRectangle(handleRect, 2.0f);

                float cx = handleRect.getCentreX();
                float cy = handleRect.getCentreY();
                g.setColour(FPColours::text.withAlpha(0.6f));
                for (int i = -2; i <= 2; ++i)
                    g.fillEllipse(cx + (float)i * 5.0f - 1.0f, cy - 1.0f, 2.0f, 2.0f);

                g.setColour(FPColours::text.withAlpha(0.35f));
                g.drawVerticalLine((int)handleRect.getX() + 1, handleRect.getY() + 1, handleRect.getBottom() - 1);
                g.drawVerticalLine((int)handleRect.getRight() - 2, handleRect.getY() + 1, handleRect.getBottom() - 1);
            }
        }
    }

}

void PianoRoll::drawPlayhead(Graphics& g)
{
    float x = xForBeat(playheadBeat);
    g.setColour(FPColours::playhead);
    g.fillRect(x - 1.0f, 0.0f, 2.0f, (float)getHeight());
}

void PianoRoll::drawFreehandPreview(Graphics& g)
{
    if (!curveDrawer.isDrawing()) return;

    auto& points = curveDrawer.getDrawnPoints();
    if (points.empty()) return;

    auto* note = curveDrawer.getTargetNote();
    if (note == nullptr) return;

    auto bounds = boundsForNote(*note);
    float pps = getPixelsPerSemitone();

    Path previewPath;
    bool started = false;

    for (auto& pt : points)
    {
        float x = bounds.getX() + (float)(pt.time / note->durationBeats) * bounds.getWidth();
        float y = bounds.getCentreY() - (float)pt.pitchOffset * pps;

        if (!started) { previewPath.startNewSubPath(x, y); started = true; }
        else previewPath.lineTo(x, y);
    }

    g.setColour(FPColours::pitchCurveAlt.withAlpha(0.6f));
    g.strokePath(previewPath, PathStrokeType(2.0f, PathStrokeType::curved));

    auto& lastPt = points.back();
    float lastX = bounds.getX() + (float)(lastPt.time / note->durationBeats) * bounds.getWidth();
    float lastY = bounds.getCentreY() - (float)lastPt.pitchOffset * pps;
    float curX = bounds.getX() + (float)(curveDrawer.getCursorTime() / note->durationBeats) * bounds.getWidth();
    float curY = bounds.getCentreY() - curveDrawer.getCursorPitch() * pps;

    if (std::abs(curX - lastX) > 1.0f || std::abs(curY - lastY) > 1.0f)
    {
        g.setColour(FPColours::pitchCurveAlt.withAlpha(0.35f));
        g.drawLine(lastX, lastY, curX, curY, 1.5f);

        g.setColour(FPColours::pitchCurveAlt.withAlpha(0.5f));
        g.fillEllipse(curX - 3.0f, curY - 3.0f, 6.0f, 6.0f);
    }
}

void PianoRoll::drawHoverFeedback(Graphics& g)
{
    
    if (lineHoverMode == LineHoverMode::OnLine && hoveredSegmentNote != nullptr)
    {
        float px = previewDotPos.x;
        float py = previewDotPos.y;

        g.setColour(FPColours::pitchCurveAlt.withAlpha(0.3f));
        g.fillEllipse(px - 4, py - 4, 8, 8);

        g.setColour(FPColours::playhead.withAlpha(0.2f));
        g.drawEllipse(px - 5, py - 5, 10, 10, 1.0f);
        return;
    }

    if (hoveredPointNote == nullptr || hoveredPointIndex < 0) return;
    if (hoveredPointIndex >= (int)hoveredPointNote->pitchCurve.size()) return;

    auto bounds = boundsForNote(*hoveredPointNote);
    float pps = getPixelsPerSemitone();
    auto& pt = hoveredPointNote->pitchCurve[(size_t)hoveredPointIndex];

    float px = bounds.getX() + (float)(pt.time / hoveredPointNote->durationBeats) * bounds.getWidth();
    float py = bounds.getCentreY() - (float)pt.pitchOffset * pps;

    g.setColour(FPColours::pitchCurveAlt.withAlpha(0.25f));
    g.fillEllipse(px - 5, py - 5, 10, 10);

    g.setColour(FPColours::playhead.withAlpha(0.35f));
    g.drawEllipse(px - 6, py - 6, 12, 12, 1.0f);
}

void PianoRoll::paint(Graphics& g)
{
    g.fillAll(FPColours::surfaceLight);
    drawGrid(g);
    drawDefaultPitchLines(g);
    drawNotes(g);
    drawFreehandPreview(g);
    if (currentTool == EditorToolbar::Tool::Vibrato)
        drawVibratoHandles(g);
    drawExpressionPreview(g);
    drawRecordingPreview(g);
    drawMarqueeSelection(g);
    drawHoverFeedback(g);

    if (cutDragging)
    {
        Point<float> p0 = mouseDownPos;
        Point<float> p1 = cutLineEnd;

        g.setColour(Colour(0x40ff5050));
        g.drawLine(p0.x, p0.y, p1.x, p1.y, 5.0f);
        
        g.setColour(Colour(0xffff6060));
        g.drawLine(p0.x, p0.y, p1.x, p1.y, 1.8f);

        float dx = p1.x - p0.x, dy = p1.y - p0.y;
        for (auto& n : noteSequence.getAllNotes())
        {
            auto b = boundsForNote(n);
            float cutX;
            if (std::abs(dx) < 0.5f) cutX = p0.x;
            else if (std::abs(dy) < 0.5f) cutX = p0.x + dx * 0.5f;
            else {
                float t = (b.getCentreY() - p0.y) / dy;
                if (t < 0.0f || t > 1.0f) continue;
                cutX = p0.x + t * dx;
            }
            if (cutX <= b.getX() + 1.0f || cutX >= b.getRight() - 1.0f) continue;
            g.setColour(Colour(0xffffe070));
            g.fillRect(cutX - 1.0f, b.getY(), 2.0f, b.getHeight());
        }
    }
    drawPlayhead(g);
    drawRecordingOverlay(g);

    if (!multiSelection.empty())
    {
        for (auto& sel : multiSelection)
        {
            if (sel.pointIndex >= 0 && sel.pointIndex < (int)sel.note->pitchCurve.size())
            {
                
                auto bounds = boundsForNote(*sel.note);
                auto& pt = sel.note->pitchCurve[(size_t)sel.pointIndex];
                float px = bounds.getX() + (float)(pt.time / sel.note->durationBeats) * bounds.getWidth();
                float py = bounds.getCentreY() - (float)pt.pitchOffset * getPixelsPerSemitone();

                g.setColour(FPColours::accentCyan.withAlpha(0.7f));
                g.drawEllipse(px - 6.0f, py - 6.0f, 12.0f, 12.0f, 1.5f);
            }
            else if (sel.pointIndex < 0)
            {
                
                auto bounds = boundsForNote(*sel.note);
                g.setColour(FPColours::accentCyan.withAlpha(0.3f));
                g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), handleHeight);
                g.setColour(FPColours::accentCyan.withAlpha(0.7f));
                g.drawRect(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 1.5f);
            }
        }

        drawSelectionBoundingBox(g);
    }

    if (hoveredNote != nullptr && hoveredPointIndex < 0 && gamakaStampName.isEmpty()
        && currentTool == EditorToolbar::Tool::Edit)
    {
        auto mousePos = getMouseXYRelative();
        double beat = beatAtX((float)mousePos.x);
        float pitch = hoveredNote->getPitchAtTime(beat);
        int cents = (int)((pitch - std::floor(pitch)) * 100);
        String info = String("Note: ") + String(hoveredNote->noteNumber)
                          + " Pitch: " + String(pitch, 2)
                          + " (" + String(cents) + "c)";

        g.setColour(FPColours::surface.withAlpha(0.9f));
        g.fillRoundedRectangle((float)mousePos.x + 10, (float)mousePos.y - 25, 180, 20, 4);
        g.setColour(FPColours::text);
        g.setFont(FontOptions(11.0f));
        g.drawText(info, (int)mousePos.x + 14, mousePos.y - 25, 176, 20, Justification::centredLeft);
    }

    paintHorizontalScrollbar(g);
}

void PianoRoll::drawRecordingOverlay(Graphics& g)
{
    if (!isRecording) return;

    g.setColour(Colour(0x0cff2020));
    g.fillRect(getLocalBounds());

    g.setColour(Colour(0x40ff3030));
    g.drawRect(getLocalBounds(), 2);

    g.setColour(Colour(0xccff3030));
    g.fillEllipse(10.0f, 8.0f, 8.0f, 8.0f);  
    g.setFont(FontOptions(12.0f, Font::bold));
    g.drawText("REC", 22, 5, 40, 14, Justification::centredLeft);
}

void PianoRoll::drawRecordingPreview(Graphics& g)
{
    if (recordingPreviewNotes.empty()) return;

    float pps = getPixelsPerSemitone();

    for (auto& rn : recordingPreviewNotes)
    {
        float x1 = xForBeat(rn.startBeat);
        float x2 = xForBeat(rn.startBeat + rn.durationBeats);
        float y = yForNote((double)rn.noteNumber + 1);
        float noteH = yForNote((double)rn.noteNumber) - y;

        if (x2 < 0 || x1 > (float)getWidth()) continue;

        g.setColour(Colour(0x55ff6030));
        g.fillRoundedRectangle(x1, y, x2 - x1, noteH, 2.0f);

        g.setColour(Colour(0x88ff6030));
        g.drawRoundedRectangle(x1, y, x2 - x1, noteH, 2.0f, 1.0f);

        if (rn.pitchCurve.size() >= 2)
        {
            float centreY = y + noteH * 0.5f;
            Path curvePath;
            bool started = false;

            for (auto& pt : rn.pitchCurve)
            {
                float px = xForBeat(rn.startBeat + pt.relTime);
                float py = centreY - pt.offset * pps;

                if (!started) { curvePath.startNewSubPath(px, py); started = true; }
                else curvePath.lineTo(px, py);
            }

            g.setColour(Colour(0xccff8040));
            g.strokePath(curvePath, PathStrokeType(1.5f));
        }
    }
}

void PianoRoll::drawExpressionPreview(Graphics& g)
{
    if (gamakaStampName.isEmpty() || hoveredNote == nullptr) return;

    auto noteBounds = boundsForNote(*hoveredNote);
    auto exprPoints = generateGamakaForNote(gamakaStampName, hoveredNote, gamakaStampIntensity);

    if (exprPoints.empty()) return;

    std::vector<PitchPoint> mergedCurve = hoveredNote->pitchCurve;
    if (mergedCurve.empty())
        mergedCurve = exprPoints;
    else
        PitchCurveInterpolator::mergeIntoCurve(mergedCurve, exprPoints, 0.0);

    float pps = getPixelsPerSemitone();
    float baseY = noteBounds.getCentreY();
    bool hasExisting = !hoveredNote->pitchCurve.empty();

    Path ghostPath;
    int steps = jmax(60, (int)(noteBounds.getWidth() * 2));
    for (int s = 0; s <= steps; ++s)
    {
        float frac = (float)s / (float)steps;
        double t = frac * hoveredNote->durationBeats;
        float pitch = PitchCurveInterpolator::interpolate(mergedCurve, t, false);
        float x = noteBounds.getX() + frac * noteBounds.getWidth();
        float y = baseY - pitch * pps;
        if (s == 0) ghostPath.startNewSubPath(x, y);
        else ghostPath.lineTo(x, y);
    }

    g.setColour(FPColours::gamaka.withAlpha(0.55f));
    g.strokePath(ghostPath, PathStrokeType(2.0f));

    Path fillPath(ghostPath);
    for (int s = steps; s >= 0; --s)
    {
        float frac = (float)s / (float)steps;
        double t = frac * hoveredNote->durationBeats;
        float existingPitch = 0.0f;
        if (hasExisting)
            existingPitch = PitchCurveInterpolator::interpolate(hoveredNote->pitchCurve, t, false);
        float x = noteBounds.getX() + frac * noteBounds.getWidth();
        float y = baseY - existingPitch * pps;
        fillPath.lineTo(x, y);
    }
    fillPath.closeSubPath();
    g.setColour(FPColours::gamaka.withAlpha(0.1f));
    g.fillPath(fillPath);
}

void PianoRoll::drawMarqueeSelection(Graphics& g)
{
    if (dragMode != DragMode::MarqueeSelect) return;
    if (marqueeRect.getWidth() < 2.0f && marqueeRect.getHeight() < 2.0f) return;

    g.setColour(FPColours::accentCyan.withAlpha(0.08f));
    g.fillRect(marqueeRect);

    g.setColour(FPColours::accentCyan.withAlpha(0.5f));
    float dashes[] = { 4.0f, 3.0f };
    g.drawDashedLine(Line<float>(marqueeRect.getX(), marqueeRect.getY(), marqueeRect.getRight(), marqueeRect.getY()), dashes, 2, 1.0f);
    g.drawDashedLine(Line<float>(marqueeRect.getRight(), marqueeRect.getY(), marqueeRect.getRight(), marqueeRect.getBottom()), dashes, 2, 1.0f);
    g.drawDashedLine(Line<float>(marqueeRect.getRight(), marqueeRect.getBottom(), marqueeRect.getX(), marqueeRect.getBottom()), dashes, 2, 1.0f);
    g.drawDashedLine(Line<float>(marqueeRect.getX(), marqueeRect.getBottom(), marqueeRect.getX(), marqueeRect.getY()), dashes, 2, 1.0f);
}

Rectangle<float> PianoRoll::getMultiSelectionBoundingBox() const
{
    
    bool hasPoints = false;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;

    for (auto& sel : multiSelection)
    {
        if (sel.pointIndex < 0 || sel.pointIndex >= (int)sel.note->pitchCurve.size())
            continue;

        auto bounds = boundsForNote(*sel.note);
        auto& pt = sel.note->pitchCurve[(size_t)sel.pointIndex];
        float px = bounds.getX() + (float)(pt.time / sel.note->durationBeats) * bounds.getWidth();
        float py = bounds.getCentreY() - (float)pt.pitchOffset * getPixelsPerSemitone();

        minX = std::min(minX, px);
        maxX = std::max(maxX, px);
        minY = std::min(minY, py);
        maxY = std::max(maxY, py);
        hasPoints = true;
    }

    if (!hasPoints)
        return {};

    constexpr float pad = 10.0f;
    return { minX - pad, minY - pad, (maxX - minX) + 2 * pad, (maxY - minY) + 2 * pad };
}

PianoRoll::ScaleEdge PianoRoll::findScaleEdgeAt(float mx, float my) const
{
    auto bb = getMultiSelectionBoundingBox();
    if (bb.isEmpty()) return ScaleEdge::None;

    constexpr float hitZone = 6.0f;

    bool inVertRange = my >= bb.getY() - hitZone && my <= bb.getBottom() + hitZone;
    bool inHorizRange = mx >= bb.getX() - hitZone && mx <= bb.getRight() + hitZone;

    if (inVertRange && std::abs(mx - bb.getX()) <= hitZone)
        return ScaleEdge::Left;
    
    if (inVertRange && std::abs(mx - bb.getRight()) <= hitZone)
        return ScaleEdge::Right;
    
    if (inHorizRange && std::abs(my - bb.getY()) <= hitZone)
        return ScaleEdge::Top;
    
    if (inHorizRange && std::abs(my - bb.getBottom()) <= hitZone)
        return ScaleEdge::Bottom;

    return ScaleEdge::None;
}

void PianoRoll::drawSelectionBoundingBox(Graphics& g)
{
    if (multiSelection.size() < 2) return;

    int dotCount = 0;
    for (auto& sel : multiSelection)
        if (sel.pointIndex >= 0) dotCount++;
    if (dotCount < 2) return;

    auto bb = getMultiSelectionBoundingBox();
    if (bb.isEmpty()) return;

    g.setColour(FPColours::accentCyan.withAlpha(0.12f));
    g.fillRect(bb);

    g.setColour(FPColours::accentCyan.withAlpha(0.6f));
    g.drawRect(bb, 1.0f);

    constexpr float handleW = 8.0f;
    constexpr float handleH = 8.0f;
    auto handleColour = FPColours::accentCyan.withAlpha(0.9f);

    auto drawHandle = [&](float cx, float cy, bool isHovered)
    {
        auto col = isHovered ? Colours::white : handleColour;
        g.setColour(col);
        g.fillRect(cx - handleW / 2, cy - handleH / 2, handleW, handleH);
        g.setColour(col.darker(0.3f));
        g.drawRect(cx - handleW / 2, cy - handleH / 2, handleW, handleH, 1.0f);
    };

    drawHandle(bb.getX(), bb.getCentreY(), hoveredScaleEdge == ScaleEdge::Left);
    
    drawHandle(bb.getRight(), bb.getCentreY(), hoveredScaleEdge == ScaleEdge::Right);
    
    drawHandle(bb.getCentreX(), bb.getY(), hoveredScaleEdge == ScaleEdge::Top);
    
    drawHandle(bb.getCentreX(), bb.getBottom(), hoveredScaleEdge == ScaleEdge::Bottom);
}

void PianoRoll::resized()
{
    scrollbarY = (float)(getHeight() - (int)scrollbarHeight);
}

void PianoRoll::paintHorizontalScrollbar(Graphics& g)
{
    float w = (float)getWidth();
    float barW = w;
    float barY = scrollbarY;

    g.setColour(FPColours::surface);
    g.fillRect(0.0f, barY, barW, scrollbarHeight);

    double beatRange = viewEndBeat - viewStartBeat;
    if (beatRange <= 0.0 || totalContentBeats <= 0.0) return;

    scrollbarThumbW = jmax(20.0f, (float)(beatRange / totalContentBeats) * barW);
    float maxThumbX = barW - scrollbarThumbW;
    float thumbFrac = 0.0;
    if (totalContentBeats > beatRange)
        thumbFrac = (float)(viewStartBeat / (totalContentBeats - beatRange));
    scrollbarThumbX = thumbFrac * maxThumbX;

    g.setColour(FPColours::gridLineBright);
    g.fillRoundedRectangle(scrollbarThumbX, barY + 1.0f, scrollbarThumbW, scrollbarHeight - 2.0f, 3.0f);
}

void PianoRoll::mouseDownScrollbar(const MouseEvent& e)
{
    float mx = (float)e.x;
    if (mx >= scrollbarThumbX && mx <= scrollbarThumbX + scrollbarThumbW)
    {
        scrollbarDragging = true;
        scrollbarThumbX = mx - scrollbarThumbW * 0.5f;
    }
    else
    {
        scrollbarDragging = true;
        scrollbarThumbX = jlimit(0.0f, (float)getWidth() - scrollbarThumbW, mx - scrollbarThumbW * 0.5f);
    }
    updateBeatFromScrollbar();
    repaint();
}

void PianoRoll::mouseDragScrollbar(const MouseEvent& e)
{
    if (!scrollbarDragging) return;
    float maxThumbX = (float)getWidth() - scrollbarThumbW;
    scrollbarThumbX = jlimit(0.0f, maxThumbX, (float)e.x - scrollbarThumbW * 0.5f);
    updateBeatFromScrollbar();
    repaint();
}

void PianoRoll::updateBeatFromScrollbar()
{
    float maxThumbX = (float)getWidth() - scrollbarThumbW;
    if (maxThumbX <= 0.0f) return;
    double frac = (double)(scrollbarThumbX / maxThumbX);
    double beatRange = viewEndBeat - viewStartBeat;
    double maxStart = totalContentBeats - beatRange;
    if (maxStart < 0.0) maxStart = 0.0;
    viewStartBeat = frac * maxStart;
    viewEndBeat = viewStartBeat + beatRange;
    if (onViewChanged) onViewChanged();
}

void PianoRoll::eraseDotsNear(float mx, float my)
{
    float pps = getPixelsPerSemitone();
    float hitRadius = 10.0f;
    float bestDist = hitRadius * hitRadius;
    NoteData* bestNote = nullptr;
    int bestIdx = -1;

    for (auto& note : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(note);
        if (mx < bounds.getX() - 10 || mx > bounds.getRight() + 10)
            continue;

        for (int i = 0; i < (int)note.pitchCurve.size(); ++i)
        {
            auto& pt = note.pitchCurve[(size_t)i];
            float px = bounds.getX() + (float)(pt.time / note.durationBeats) * bounds.getWidth();
            float py = bounds.getCentreY() - (float)pt.pitchOffset * pps;
            float dx = mx - px;
            float dy = my - py;
            float dist = dx * dx + dy * dy;
            if (dist < bestDist)
            {
                bestDist = dist;
                bestNote = &note;
                bestIdx = i;
            }
        }
    }

    if (bestNote != nullptr && bestIdx >= 0)
    {
        if (!rightClickUndoSaved)
        {
            saveUndoState();
            rightClickUndoSaved = true;
        }
        bestNote->pitchCurve.erase(bestNote->pitchCurve.begin() + bestIdx);
        if (onNotesChanged) onNotesChanged();
        repaint();
    }
}

std::pair<NoteData, NoteData> PianoRoll::buildSplitPair(const NoteData& src, double absoluteCutBeat) const
{
    const double cutRel = jlimit(0.0, src.durationBeats, absoluteCutBeat - src.startBeat);

    float cutValue = 0.0f;
    if (!src.pitchCurve.empty())
        cutValue = PitchCurveInterpolator::interpolate(src.pitchCurve, cutRel, false);

    int segIdx = -1;
    for (int i = 0; i + 1 < (int) src.pitchCurve.size(); ++i)
    {
        const auto& a = src.pitchCurve[(size_t) i];
        const auto& b = src.pitchCurve[(size_t) (i + 1)];
        if (cutRel >= a.time && cutRel <= b.time) { segIdx = i; break; }
    }

    NoteData A = src;
    A.durationBeats = cutRel;
    A.selected = false;
    A.pitchCurve.clear();

    for (auto& pt : src.pitchCurve)
    {
        if (pt.time < cutRel - 1.0e-6)
            A.pitchCurve.push_back(pt);
    }
    
    if (segIdx >= 0)
    {
        PitchPoint term;
        term.time = cutRel;
        term.pitchOffset = cutValue;
        term.curveType = src.pitchCurve[(size_t) segIdx].curveType;
        term.curvature = 1.0f;
        term.bias = 0.0f;
        
        A.pitchCurve.push_back(term);
    }
    else if (!src.pitchCurve.empty() && cutRel >= src.pitchCurve.back().time)
    {
        
        PitchPoint term;
        term.time = cutRel;
        term.pitchOffset = cutValue;
        A.pitchCurve.push_back(term);
    }

    NoteData B = src;
    B.startBeat = src.startBeat + cutRel;
    B.durationBeats = src.durationBeats - cutRel;
    B.selected = false;
    B.pitchCurve.clear();

    if (segIdx >= 0)
    {
        const auto& segStart = src.pitchCurve[(size_t) segIdx];
        PitchPoint first;
        first.time = 0.0;
        first.pitchOffset = cutValue;
        first.curveType = segStart.curveType;
        first.curvature = segStart.curvature;
        first.bias = segStart.bias;
        first.vibrato = segStart.vibrato; 
        B.pitchCurve.push_back(first);

        for (int i = segIdx + 1; i < (int) src.pitchCurve.size(); ++i)
        {
            const auto& pt = src.pitchCurve[(size_t) i];
            if (pt.time > cutRel + 1.0e-6)
            {
                PitchPoint q = pt;
                q.time = pt.time - cutRel;
                B.pitchCurve.push_back(q);
            }
        }
    }
    else if (!src.pitchCurve.empty())
    {
        
        PitchPoint first;
        first.time = 0.0;
        first.pitchOffset = cutValue;
        B.pitchCurve.push_back(first);
        for (auto& pt : src.pitchCurve)
        {
            if (pt.time > cutRel + 1.0e-6)
            {
                PitchPoint q = pt;
                q.time = pt.time - cutRel;
                B.pitchCurve.push_back(q);
            }
        }
    }

    return { A, B };
}

void PianoRoll::mouseDown(const MouseEvent& e)
{
    if ((float)e.y >= scrollbarY)
    {
        mouseDownScrollbar(e);
        return;
    }

    if (onActivated) onActivated();

    float mx = (float)e.x;
    float my = (float)e.y;
    double beat = beatAtX(mx);

    NoteData* clickedNote = findNoteAt(mx, my);
    mouseDownPos = { mx, my };
    deselectedOnDown = false;
    pointClickedOnDown = false;
    clickedPointNote = nullptr;
    clickedPointIndex = -1;
    dragMode = DragMode::None;
    undoSavedForDrag = false;
    duplicatedForDrag = false;
    draggedPointIndex = -1;

    if (e.mods.isRightButtonDown() && e.mods.isCommandDown() && clickedNote == nullptr)
    {
        
        NoteData* cpNote = nullptr;
        int cpIdx = -1;
        if (!findAnyControlPoint(mx, my, cpNote, cpIdx))
        {
            cutDragging = true;
            cutLineEnd = { mx, my };
            repaint();
            return;
        }
    }

    if (e.mods.isRightButtonDown())
    {
        rightClickErasing = true;
        rightClickUndoSaved = false;

        if (!multiSelection.empty())
        {
            saveUndoState();
            rightClickUndoSaved = true;

            std::vector<NoteData*> notesToDelete;
            
            std::map<NoteData*, std::vector<int>> dotsToDelete;
            for (auto& sel : multiSelection)
            {
                if (sel.pointIndex < 0)
                    notesToDelete.push_back(sel.note);
                else
                    dotsToDelete[sel.note].push_back(sel.pointIndex);
            }

            for (auto& [note, indices] : dotsToDelete)
            {
                std::sort(indices.begin(), indices.end(), std::greater<int>());
                for (int idx : indices)
                {
                    if (idx < (int)note->pitchCurve.size())
                        note->pitchCurve.erase(note->pitchCurve.begin() + idx);
                }
            }

            for (auto* note : notesToDelete)
            {
                auto& notes = noteSequence.getAllNotes();
                for (int i = 0; i < (int)notes.size(); ++i)
                {
                    if (&notes[(size_t)i] == note)
                    {
                        if (selectedNote == note) selectedNote = nullptr;
                        noteSequence.removeNote(i);
                        break;
                    }
                }
            }

            clearMultiSelection();
            rightClickErasing = false;
            if (onNotesChanged) onNotesChanged();
            repaint();
            return;
        }

        NoteData* cpNote = nullptr;
        int cpIdx = -1;
        if (findAnyControlPoint(mx, my, cpNote, cpIdx))
        {
            saveUndoState();
            rightClickUndoSaved = true;
            cpNote->pitchCurve.erase(cpNote->pitchCurve.begin() + cpIdx);
            if (onNotesChanged) onNotesChanged();
        }
        else if (clickedNote)
        {
            saveUndoState();
            rightClickUndoSaved = true;
            auto& notes = noteSequence.getAllNotes();
            for (int i = 0; i < (int)notes.size(); ++i)
            {
                if (&notes[(size_t)i] == clickedNote)
                {
                    if (selectedNote == clickedNote) selectedNote = nullptr;
                    noteSequence.removeNote(i);
                    if (onNotesChanged) onNotesChanged();
                    break;
                }
            }
            rightClickErasing = false;
        }
        repaint();
        return;
    }

    if (gamakaStampName.isNotEmpty())
    {
        
        NoteData* cpNote = nullptr;
        int cpIdx = -1;
        bool hitDot = findAnyControlPoint(mx, my, cpNote, cpIdx);
        bool hitHandle = false;
        for (auto& note : noteSequence.getAllNotes())
        {
            if (isInHandle(note, mx, my)) { hitHandle = true; break; }
        }

        if (!hitDot && !hitHandle)
        {
            if (clickedNote)
            {
                
                saveUndoState();
                selectedNote = clickedNote;
                auto gamakaPoints = generateGamakaForNote(gamakaStampName, clickedNote, gamakaStampIntensity);

                if (clickedNote->pitchCurve.empty())
                    clickedNote->pitchCurve = gamakaPoints;
                else
                    PitchCurveInterpolator::mergeIntoCurve(clickedNote->pitchCurve, gamakaPoints, 0.0);

                if (onNotesChanged) onNotesChanged();
                if (onNoteSelected) onNoteSelected(selectedNote);
                repaint();
                return;
            }
            else
            {
                
                clearExpressionStamp();
                deselectedOnDown = true;
                
            }
        }
        
    }

    switch (currentTool)
    {
        case EditorToolbar::Tool::Edit:
        case EditorToolbar::Tool::Pencil:
        case EditorToolbar::Tool::Vibrato:
        {
            
            if (multiSelection.size() >= 2)
            {
                ScaleEdge edge = findScaleEdgeAt(mx, my);
                if (edge != ScaleEdge::None)
                {
                    saveUndoState();
                    activeScaleEdge = edge;
                    auto bb = getMultiSelectionBoundingBox();

                    multiDragOrigPositions.clear();
                    for (auto& sel : multiSelection)
                    {
                        if (sel.pointIndex >= 0 && sel.pointIndex < (int)sel.note->pitchCurve.size())
                        {
                            auto& pt = sel.note->pitchCurve[(size_t)sel.pointIndex];
                            multiDragOrigPositions.push_back({sel.note, sel.pointIndex, pt.time, pt.pitchOffset});
                        }
                    }

                    if (edge == ScaleEdge::Left || edge == ScaleEdge::Right)
                    {
                        dragMode = DragMode::ScaleHorizontal;
                        scaleDragStartMouse = mx;
                        if (edge == ScaleEdge::Left)
                        {
                            scaleDragAnchor = bb.getRight();     
                            scaleDragMovingEdge = bb.getX();     
                        }
                        else
                        {
                            scaleDragAnchor = bb.getX();         
                            scaleDragMovingEdge = bb.getRight(); 
                        }
                    }
                    else
                    {
                        dragMode = DragMode::ScaleVertical;
                        scaleDragStartMouse = my;
                        if (edge == ScaleEdge::Top)
                        {
                            scaleDragAnchor = bb.getBottom();    
                            scaleDragMovingEdge = bb.getY();     
                        }
                        else
                        {
                            scaleDragAnchor = bb.getY();         
                            scaleDragMovingEdge = bb.getBottom(); 
                        }
                    }
                    break;
                }
            }

            if (e.mods.isAltDown())
            {
                NoteData* cpNote = nullptr;
                int cpIdx = -1;
                if (findAnyControlPoint(mx, my, cpNote, cpIdx))
                {
                    pointClickedOnDown = false;
                    selectedNote = cpNote;
                    draggedPointIndex = cpIdx;
                    dragMode = DragMode::DragPoint;
                    clearMultiSelection();
                    break;
                }

                NoteData* segNote = nullptr;
                int segIdx = -1;
                if (findCurveSegmentAt(mx, my, segNote, segIdx))
                {
                    dragMode = DragMode::AdjustCurvature;
                    curvatureDragNote = segNote;
                    curvatureDragSegIndex = segIdx;
                    curvatureDragStartValue = segNote->pitchCurve[(size_t)segIdx].bias;
                    selectedNote = segNote;
                    clearMultiSelection();
                    break;
                }
            }

            if (currentTool == EditorToolbar::Tool::Vibrato)
            {
                NoteData* handleNote = nullptr;
                int handleSeg = -1;
                VibratoHandle handle = findVibratoHandleAt(mx, my, handleNote, handleSeg);
                if (handle != VibratoHandle::None)
                {
                    selectedNote = handleNote;
                    auto& vib = handleNote->pitchCurve[(size_t)handleSeg].vibrato;

                    if (handle == VibratoHandle::Waveform)
                    {
                        saveUndoState();
                        int next = ((int)vib.waveform + 1) % (int)VibratoWaveform::NumTypes;
                        vib.waveform = (VibratoWaveform)next;
                        if (onNotesChanged) onNotesChanged();
                        break;
                    }

                    vibratoDragNote = handleNote;
                    vibratoDragSegIndex = handleSeg;
                    vibratoHandleDragging = handle;
                    dragMode = DragMode::VibratoHandle;

                    switch (handle)
                    {
                        case VibratoHandle::Depth:    vibratoDragStartValue = vib.depth; break;
                        case VibratoHandle::Rate:     vibratoDragStartValue = vib.rate; break;
                        case VibratoHandle::FadeIn:   vibratoDragStartValue = vib.fadeInFrac; break;
                        case VibratoHandle::FadeOut:  vibratoDragStartValue = vib.fadeOutFrac; break;
                        case VibratoHandle::Offset:   vibratoDragStartValue = vib.offset; break;
                        case VibratoHandle::Waveform: break;
                        case VibratoHandle::None:     break;
                    }
                    break;
                }
            }

            {
                NoteData* cpNote = nullptr;
                int cpIdx = -1;
                if (findAnyControlPoint(mx, my, cpNote, cpIdx))
                {
                    
                    if (isInMultiSelection(cpNote, cpIdx))
                    {
                        dragMode = DragMode::MarqueeMove;
                        multiDragStart = { mx, my };
                        
                        multiDragOrigPositions.clear();
                        for (auto& sel : multiSelection)
                        {
                            if (sel.pointIndex >= 0 && sel.pointIndex < (int)sel.note->pitchCurve.size())
                            {
                                auto& pt = sel.note->pitchCurve[(size_t)sel.pointIndex];
                                multiDragOrigPositions.push_back({sel.note, sel.pointIndex, pt.time, pt.pitchOffset});
                            }
                            else if (sel.pointIndex < 0)
                            {
                                multiDragOrigPositions.push_back({sel.note, -1, sel.note->startBeat, (double)sel.note->noteNumber});
                            }
                        }
                        break;
                    }

                    pointClickedOnDown = true;
                    clickedPointNote = cpNote;
                    clickedPointIndex = cpIdx;
                    selectedNote = cpNote;
                    draggedPointIndex = cpIdx;
                    dragMode = DragMode::DragPoint;
                    clearMultiSelection();
                    break;
                }
            }

            if (!e.mods.isAltDown() && lineHoverMode != LineHoverMode::None && hoveredSegmentNote != nullptr)
            {
                if (lineHoverMode == LineHoverMode::OnLine)
                {
                    
                    saveUndoState();
                    selectedNote = hoveredSegmentNote;
                    clearMultiSelection();

                    auto bounds = boundsForNote(*hoveredSegmentNote);
                    double relTime = (double)(mx - bounds.getX()) / (double)bounds.getWidth() * hoveredSegmentNote->durationBeats;
                    relTime = jlimit(0.0, hoveredSegmentNote->durationBeats, relTime);
                    float pitchOff = PitchCurveInterpolator::interpolate(hoveredSegmentNote->pitchCurve, relTime);

                    PitchPoint pt;
                    pt.time = relTime;
                    pt.pitchOffset = pitchOff;
                    pt.curveType = PitchPoint::CurveType::Smooth;
                    hoveredSegmentNote->pitchCurve.push_back(pt);

                    std::sort(hoveredSegmentNote->pitchCurve.begin(), hoveredSegmentNote->pitchCurve.end(),
                              [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });

                    draggedPointIndex = findControlPointAt(hoveredSegmentNote, mx, previewDotPos.y);
                    dragMode = DragMode::DragPoint;
                    pointClickedOnDown = false;  
                    lineHoverMode = LineHoverMode::None;

                    if (onNotesChanged) onNotesChanged();
                    break;
                }
                else if (lineHoverMode == LineHoverMode::Near)
                {
                    
                    selectedNote = hoveredSegmentNote;
                    lineDragNote = hoveredSegmentNote;
                    clearMultiSelection();

                    lineDragOrigPositions.clear();
                    for (auto& pt : lineDragNote->pitchCurve)
                        lineDragOrigPositions.push_back({ pt.time, pt.pitchOffset });

                    dragMode = DragMode::MoveLine;
                    lineHoverMode = LineHoverMode::None;
                    break;
                }
            }

            if (currentTool == EditorToolbar::Tool::Vibrato)
            {
                NoteData* segNote = nullptr;
                int segIdx = -1;
                if (findCurveSegmentAt(mx, my, segNote, segIdx))
                {
                    saveUndoState();
                    selectedNote = segNote;
                    segNote->pitchCurve[(size_t)segIdx].vibrato.enabled =
                        !segNote->pitchCurve[(size_t)segIdx].vibrato.enabled;
                    clearMultiSelection();
                    if (onNotesChanged) onNotesChanged();
                    break;
                }
            }

            for (auto& note : noteSequence.getAllNotes())
            {
                if (isInHandle(note, mx, my))
                {
                    selectedNote = &note;
                    auto bounds = boundsForNote(note);
                    float distFromLeft = mx - bounds.getX();
                    float distFromRight = bounds.getRight() - mx;

                    if (distFromLeft <= edgeThreshold)
                    {
                        dragMode = DragMode::ResizeStart;
                        noteDragStartBeat = note.startBeat;
                        noteDragOrigDuration = note.durationBeats;
                    }
                    else if (distFromRight <= edgeThreshold)
                    {
                        dragMode = DragMode::ResizeEnd;
                        noteDragStartBeat = note.startBeat;
                        noteDragOrigDuration = note.durationBeats;
                    }
                    else
                    {
                        
                        if (isInMultiSelection(&note, -1))
                        {
                            dragMode = DragMode::MarqueeMove;
                            multiDragStart = { mx, my };
                            multiDragOrigPositions.clear();
                            for (auto& sel : multiSelection)
                            {
                                if (sel.pointIndex >= 0 && sel.pointIndex < (int)sel.note->pitchCurve.size())
                                {
                                    auto& pt = sel.note->pitchCurve[(size_t)sel.pointIndex];
                                    multiDragOrigPositions.push_back({sel.note, sel.pointIndex, pt.time, pt.pitchOffset});
                                }
                                else if (sel.pointIndex < 0)
                                {
                                    multiDragOrigPositions.push_back({sel.note, -1, sel.note->startBeat, (double)sel.note->noteNumber});
                                }
                            }
                            break;
                        }

                        dragMode = DragMode::MoveNote;
                        noteDragStartBeat = note.startBeat;
                        noteDragStartNote = note.noteNumber;
                        noteDragOrigDuration = note.durationBeats;
                    }
                    clearMultiSelection();
                    break;
                }
            }
            if (dragMode != DragMode::None)
                break;

            if (clickedNote)
            {
                selectedNote = clickedNote;
                clearMultiSelection();

                if (currentTool == EditorToolbar::Tool::Vibrato)
                {
                    dragMode = DragMode::MoveNote;
                    noteDragStartBeat = clickedNote->startBeat;
                    noteDragStartNote = clickedNote->noteNumber;
                    noteDragOrigDuration = clickedNote->durationBeats;
                    break;
                }

                saveUndoState();
                double relTime = beat - clickedNote->startBeat;
                auto bounds = boundsForNote(*clickedNote);
                float pps = getPixelsPerSemitone();
                float pitchOff = (bounds.getCentreY() - my) / pps;

                if (currentTool == EditorToolbar::Tool::Pencil)
                {
                    
                    curveDrawer.beginDrawing(clickedNote, relTime, pitchOff);
                }
                else
                {
                    
                    PitchPoint pt;
                    pt.time = relTime;
                    pt.pitchOffset = pitchOff;
                    pt.curveType = PitchPoint::CurveType::Smooth;
                    clickedNote->pitchCurve.push_back(pt);

                    std::sort(clickedNote->pitchCurve.begin(), clickedNote->pitchCurve.end(),
                              [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });

                    draggedPointIndex = findControlPointAt(clickedNote, mx, my);
                    dragMode = DragMode::DragPoint;
                }
                if (onNotesChanged) onNotesChanged();
                break;
            }

            if (!multiSelection.empty())
            {
                clearMultiSelection();
                selectedNote = nullptr;
                deselectedOnDown = true;
                if (onNoteSelected) onNoteSelected(nullptr);
                dragMode = DragMode::MarqueeSelect;
                marqueeRect = { mx, my, 0.0f, 0.0f };
                marqueeActive = true;
                break;
            }

            if (selectedNote != nullptr)
            {
                double noteStartX = (double)xForBeat(selectedNote->startBeat);
                double noteEndX = (double)xForBeat(selectedNote->getEndBeat());

                if ((double)mx >= noteStartX - 5.0 && (double)mx <= noteEndX + 5.0)
                {
                    
                    if (currentTool == EditorToolbar::Tool::Vibrato)
                        break;

                    saveUndoState();
                    double relTime = beat - selectedNote->startBeat;
                    relTime = jlimit(0.0, selectedNote->durationBeats, relTime);
                    auto bounds = boundsForNote(*selectedNote);
                    float pps = getPixelsPerSemitone();
                    float pitchOff = (bounds.getCentreY() - my) / pps;

                    PitchPoint pt;
                    pt.time = relTime;
                    pt.pitchOffset = pitchOff;
                    pt.curveType = PitchPoint::CurveType::Smooth;
                    selectedNote->pitchCurve.push_back(pt);

                    std::sort(selectedNote->pitchCurve.begin(), selectedNote->pitchCurve.end(),
                              [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });

                    draggedPointIndex = findControlPointAt(selectedNote, mx, my);
                    dragMode = DragMode::DragPoint;
                    if (onNotesChanged) onNotesChanged();
                }
                else
                {
                    
                    selectedNote = nullptr;
                    deselectedOnDown = true;
                    if (onNoteSelected) onNoteSelected(nullptr);
                    dragMode = DragMode::MarqueeSelect;
                    marqueeRect = { mx, my, 0.0f, 0.0f };
                    marqueeActive = true;
                }
                break;
            }

            {
                dragMode = DragMode::MarqueeSelect;
                marqueeRect = { mx, my, 0.0f, 0.0f };
                marqueeActive = true;
            }
            break;
        }
    }

    if (onNoteSelected)
        onNoteSelected(selectedNote);

    repaint();
}

void PianoRoll::mouseDrag(const MouseEvent& e)
{
    if (scrollbarDragging)
    {
        mouseDragScrollbar(e);
        return;
    }

    float mx = (float)e.x;
    float my = (float)e.y;
    double beat = beatAtX(mx);

    float dist = mouseDownPos.getDistanceFrom({ mx, my });
    bool pastThreshold = dist > dragThreshold;

    if (cutDragging)
    {
        cutLineEnd = { mx, my };
        repaint();
        return;
    }

    if (e.mods.isRightButtonDown() && rightClickErasing)
    {
        eraseDotsNear(mx, my);
        return;
    }

    if (dragMode == DragMode::MarqueeSelect)
    {
        float x0 = std::min(mouseDownPos.x, mx);
        float y0 = std::min(mouseDownPos.y, my);
        float x1 = std::max(mouseDownPos.x, mx);
        float y1 = std::max(mouseDownPos.y, my);
        marqueeRect = { x0, y0, x1 - x0, y1 - y0 };
        updateMarqueeSelection();
        repaint();
        return;
    }

    if (dragMode == DragMode::MarqueeMove && !multiDragOrigPositions.empty())
    {
        if (!pastThreshold) return;
        if (!undoSavedForDrag)
        {
            saveUndoState();
            undoSavedForDrag = true;
        }

        float deltaX = mx - multiDragStart.x;
        float deltaY = my - multiDragStart.y;
        double beatDelta = beatAtX(multiDragStart.x + deltaX) - beatAtX(multiDragStart.x);
        float pps = getPixelsPerSemitone();
        float pitchDelta = -deltaY / pps;

        for (auto& orig : multiDragOrigPositions)
        {
            if (orig.pointIdx >= 0 && orig.pointIdx < (int)orig.note->pitchCurve.size())
            {
                
                auto& pt = orig.note->pitchCurve[(size_t)orig.pointIdx];
                pt.time = jlimit(0.0, orig.note->durationBeats, orig.beat + beatDelta);
                pt.pitchOffset = orig.pitch + pitchDelta;
            }
            else if (orig.pointIdx < 0)
            {
                
                double newStart = orig.beat + beatDelta;
                newStart = snapBeat(newStart);
                orig.note->startBeat = newStart;
                int noteDelta = (int)std::round(pitchDelta);
                orig.note->noteNumber = (int)orig.pitch + noteDelta;
            }
        }

        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    if ((dragMode == DragMode::ScaleHorizontal || dragMode == DragMode::ScaleVertical)
        && !multiDragOrigPositions.empty())
    {
        if (!pastThreshold) return;

        constexpr float pad = 10.0f;

        if (dragMode == DragMode::ScaleHorizontal)
        {
            
            float origSpan = scaleDragMovingEdge - scaleDragAnchor; 
            if (std::abs(origSpan) < 1.0f) return;

            float mouseDelta = mx - scaleDragStartMouse;
            float newMovingEdge = scaleDragMovingEdge + mouseDelta;
            float newSpan = newMovingEdge - scaleDragAnchor;

            float scale = newSpan / origSpan;

            float anchorContent = (activeScaleEdge == ScaleEdge::Left)
                                    ? scaleDragAnchor - pad   
                                    : scaleDragAnchor + pad;  

            for (auto& orig : multiDragOrigPositions)
            {
                if (orig.pointIdx >= 0 && orig.pointIdx < (int)orig.note->pitchCurve.size())
                {
                    auto& pt = orig.note->pitchCurve[(size_t)orig.pointIdx];
                    auto bounds = boundsForNote(*orig.note);

                    float origPx = bounds.getX() + (float)(orig.beat / orig.note->durationBeats) * bounds.getWidth();

                    float newPx = anchorContent + (origPx - anchorContent) * scale;

                    double newRelTime = (double)(newPx - bounds.getX()) / (double)bounds.getWidth() * orig.note->durationBeats;
                    pt.time = jlimit(0.0, orig.note->durationBeats, newRelTime);

                    pt.pitchOffset = orig.pitch;
                }
            }
        }
        else 
        {
            float origSpan = scaleDragMovingEdge - scaleDragAnchor;
            if (std::abs(origSpan) < 1.0f) return;

            float mouseDelta = my - scaleDragStartMouse;
            float newMovingEdge = scaleDragMovingEdge + mouseDelta;
            float newSpan = newMovingEdge - scaleDragAnchor;

            float scale = newSpan / origSpan;

            float anchorContent = (activeScaleEdge == ScaleEdge::Top)
                                    ? scaleDragAnchor - pad   
                                    : scaleDragAnchor + pad;  

            float pps = getPixelsPerSemitone();

            for (auto& orig : multiDragOrigPositions)
            {
                if (orig.pointIdx >= 0 && orig.pointIdx < (int)orig.note->pitchCurve.size())
                {
                    auto& pt = orig.note->pitchCurve[(size_t)orig.pointIdx];
                    auto bounds = boundsForNote(*orig.note);

                    float origPy = bounds.getCentreY() - (float)orig.pitch * pps;

                    float newPy = anchorContent + (origPy - anchorContent) * scale;

                    float newPitchOff = (bounds.getCentreY() - newPy) / pps;
                    pt.pitchOffset = newPitchOff;

                    pt.time = orig.beat;
                }
            }
        }

        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    if ((currentTool == EditorToolbar::Tool::Edit || currentTool == EditorToolbar::Tool::Pencil || currentTool == EditorToolbar::Tool::Vibrato) && selectedNote != nullptr)
    {
        switch (dragMode)
        {
            case DragMode::DragPoint:
            {
                if (!pastThreshold) break;
                if (draggedPointIndex < 0 || draggedPointIndex >= (int)selectedNote->pitchCurve.size()) break;

                if (pointClickedOnDown)
                {
                    saveUndoState();
                    pointClickedOnDown = false;
                }

                if (e.mods.isAltDown())
                {
                    
                    float deltaY = mouseDownPos.y - my;
                    if (!undoSavedForDrag)
                    {
                        saveUndoState();
                        curvatureDragStartValue = selectedNote->pitchCurve[(size_t)draggedPointIndex].curvature;
                        undoSavedForDrag = true;
                    }

                    auto& pt = selectedNote->pitchCurve[(size_t)draggedPointIndex];

                    if (pt.curveType == PitchPoint::CurveType::Linear)
                        pt.curveType = PitchPoint::CurveType::Smooth;

                    float newCurv = curvatureDragStartValue + deltaY * 0.015f;
                    newCurv = jlimit(0.0f, 5.0f, newCurv);
                    pt.curvature = newCurv;
                }
                else
                {
                    
                    auto bounds = boundsForNote(*selectedNote);
                    float pps = getPixelsPerSemitone();

                    double relTime = (double)(mx - bounds.getX()) / (double)bounds.getWidth() * selectedNote->durationBeats;
                    relTime = jlimit(0.0, selectedNote->durationBeats, relTime);

                    {
                        auto& curve = selectedNote->pitchCurve;
                        int idx = draggedPointIndex;
                        double minT = (idx > 0) ? curve[(size_t)(idx - 1)].time : 0.0;
                        double maxT = (idx < (int)curve.size() - 1) ? curve[(size_t)(idx + 1)].time : selectedNote->durationBeats;
                        const double minGap = selectedNote->durationBeats * 0.005; 
                        relTime = jlimit(minT + minGap, maxT - minGap, relTime);
                    }

                    float pitchOff = (bounds.getCentreY() - my) / pps;

                    if (e.mods.isCommandDown())
                        pitchOff = std::round(pitchOff);
                    else if (e.mods.isShiftDown())
                        pitchOff = std::round(pitchOff * 2.0f) / 2.0f;

                    selectedNote->pitchCurve[(size_t)draggedPointIndex].time = relTime;
                    selectedNote->pitchCurve[(size_t)draggedPointIndex].pitchOffset = pitchOff;
                }

                if (onNotesChanged) onNotesChanged();
                repaint();
                break;
            }

            case DragMode::AdjustCurvature:
            {
                if (curvatureDragNote != nullptr && curvatureDragSegIndex >= 0
                    && curvatureDragSegIndex < (int)curvatureDragNote->pitchCurve.size())
                {
                    if (!undoSavedForDrag)
                    {
                        saveUndoState();
                        undoSavedForDrag = true;
                    }

                    float deltaY = mouseDownPos.y - my;
                    float newBias = curvatureDragStartValue + deltaY * 0.008f;
                    newBias = jlimit(-1.5f, 1.5f, newBias);

                    auto& pt = curvatureDragNote->pitchCurve[(size_t)curvatureDragSegIndex];

                    if (pt.curveType == PitchPoint::CurveType::Linear)
                        pt.curveType = PitchPoint::CurveType::Smooth;

                    pt.bias = newBias;

                    if (onNotesChanged) onNotesChanged();
                    repaint();
                }
                break;
            }

            case DragMode::MoveLine:
            {
                if (!pastThreshold) break;
                if (lineDragNote == nullptr || lineDragOrigPositions.empty()) break;

                if (!undoSavedForDrag)
                {
                    saveUndoState();
                    undoSavedForDrag = true;
                }

                float deltaX = mx - mouseDownPos.x;
                float deltaY = my - mouseDownPos.y;

                auto bounds = boundsForNote(*lineDragNote);
                float pps = getPixelsPerSemitone();

                double beatDelta = (double)deltaX / (double)bounds.getWidth() * lineDragNote->durationBeats;
                float pitchDelta = -deltaY / pps;

                for (size_t i = 0; i < lineDragNote->pitchCurve.size() && i < lineDragOrigPositions.size(); ++i)
                {
                    auto& pt = lineDragNote->pitchCurve[i];
                    auto& orig = lineDragOrigPositions[i];
                    pt.time = jlimit(0.0, lineDragNote->durationBeats, orig.time + beatDelta);

                    float newPitch = (float)(orig.pitchOffset + pitchDelta);
                    if (e.mods.isCommandDown())
                        newPitch = std::round(newPitch);
                    else if (e.mods.isShiftDown())
                        newPitch = std::round(newPitch * 2.0f) / 2.0f;
                    pt.pitchOffset = newPitch;
                }

                if (onNotesChanged) onNotesChanged();
                repaint();
                break;
            }

            case DragMode::MoveNote:
            {
                if (!undoSavedForDrag)
                {
                    saveUndoState();
                    undoSavedForDrag = true;
                }

                if (e.mods.isAltDown() && !duplicatedForDrag && pastThreshold)
                {
                    
                    NoteData duplicate = *selectedNote;
                    duplicate.startBeat = noteDragStartBeat;
                    duplicate.noteNumber = noteDragStartNote;
                    noteSequence.addNote(duplicate);
                    
                    selectedNote = nullptr;
                    for (auto& n : noteSequence.getAllNotes())
                    {
                        if (std::abs(n.startBeat - noteDragStartBeat) < 0.001
                            && n.noteNumber == noteDragStartNote
                            && &n != &noteSequence.getAllNotes().back())
                        {
                            selectedNote = &n;
                            break;
                        }
                    }
                    
                    if (selectedNote == nullptr)
                    {
                        
                        selectedNote = &noteSequence.getAllNotes().back();
                    }
                    duplicatedForDrag = true;
                }

                double beatDelta = beat - beatAtX(mouseDownPos.x);
                int noteDelta = noteAtY(my) - noteAtY(mouseDownPos.y);

                double newStart = noteDragStartBeat + beatDelta;
                newStart = snapBeat(newStart);

                selectedNote->startBeat = newStart;
                selectedNote->noteNumber = noteDragStartNote + noteDelta;

                if (onNotesChanged) onNotesChanged();
                repaint();
                break;
            }

            case DragMode::ResizeEnd:
            {
                if (!undoSavedForDrag)
                {
                    saveUndoState();
                    undoSavedForDrag = true;
                }

                double newEnd = beat;
                newEnd = snapBeat(newEnd);

                double newDuration = newEnd - selectedNote->startBeat;
                selectedNote->durationBeats = jmax(0.125, newDuration);

                if (onNotesChanged) onNotesChanged();
                repaint();
                break;
            }

            case DragMode::ResizeStart:
            {
                if (!undoSavedForDrag)
                {
                    saveUndoState();
                    undoSavedForDrag = true;
                }

                double newStart = beat;
                newStart = snapBeat(newStart);

                double origEnd = noteDragStartBeat + noteDragOrigDuration;
                double newDuration = origEnd - newStart;

                if (newDuration >= 0.125)
                {
                    selectedNote->startBeat = newStart;
                    selectedNote->durationBeats = newDuration;
                }

                if (onNotesChanged) onNotesChanged();
                repaint();
                break;
            }

            case DragMode::VibratoHandle:
                break; 

            case DragMode::MarqueeSelect:
            case DragMode::MarqueeMove:
            case DragMode::ScaleHorizontal:
            case DragMode::ScaleVertical:
                break; 

            case DragMode::None:
                break;
        }
    }

    if (dragMode == DragMode::VibratoHandle && vibratoDragNote != nullptr && vibratoDragSegIndex >= 0)
    {
        if (!undoSavedForDrag)
        {
            saveUndoState();
            undoSavedForDrag = true;
        }

        auto& vib = vibratoDragNote->pitchCurve[(size_t)vibratoDragSegIndex].vibrato;
        float deltaY = mouseDownPos.y - my;
        float deltaX = mx - mouseDownPos.x;

        auto bounds = boundsForNote(*vibratoDragNote);
        auto& ptA = vibratoDragNote->pitchCurve[(size_t)vibratoDragSegIndex];
        auto& ptB = vibratoDragNote->pitchCurve[(size_t)vibratoDragSegIndex + 1];
        float xA = bounds.getX() + (float)(ptA.time / vibratoDragNote->durationBeats) * bounds.getWidth();
        float xB = bounds.getX() + (float)(ptB.time / vibratoDragNote->durationBeats) * bounds.getWidth();
        float segPixels = jmax(1.0f, xB - xA);

        switch (vibratoHandleDragging)
        {
            case VibratoHandle::Depth:
                vib.depth = jlimit(0.02f, 3.0f, vibratoDragStartValue + deltaY * 0.006f);
                break;
            case VibratoHandle::Rate:
                vib.rate = jlimit(0.5f, 30.0f, vibratoDragStartValue + deltaY * 0.08f);
                break;
            case VibratoHandle::FadeIn:
                vib.fadeInFrac = jlimit(0.0f, 0.49f, vibratoDragStartValue + deltaX / segPixels);
                break;
            case VibratoHandle::FadeOut:
                vib.fadeOutFrac = jlimit(0.0f, 0.49f, vibratoDragStartValue - deltaX / segPixels);
                break;
            case VibratoHandle::Offset:
                vib.offset = jlimit(-3.0f, 3.0f, vibratoDragStartValue + deltaY * 0.006f);
                break;
            case VibratoHandle::Waveform: break;
            case VibratoHandle::None:     break;
        }

        if (onNotesChanged) onNotesChanged();
        repaint();
    }
    else if (curveDrawer.isDrawing())
    {
        auto* note = curveDrawer.getTargetNote();
        if (note)
        {
            double relTime = beat - note->startBeat;
            auto bounds = boundsForNote(*note);
            float pps = getPixelsPerSemitone();
            float pitchOff = (bounds.getCentreY() - my) / pps;
            curveDrawer.continueDrawing(relTime, pitchOff);
        }
        repaint();
    }
}

void PianoRoll::mouseUp(const MouseEvent& e)
{
    if (scrollbarDragging)
    {
        scrollbarDragging = false;
        return;
    }

    float mx = (float)e.x;
    float my = (float)e.y;
    float dist = mouseDownPos.getDistanceFrom({ mx, my });

    rightClickErasing = false;
    rightClickUndoSaved = false;

    if (cutDragging)
    {
        cutDragging = false;
        Point<float> p0 = mouseDownPos;
        Point<float> p1 = cutLineEnd;

        if (p0.getDistanceFrom(p1) >= dragThreshold)
        {
            float dx = p1.x - p0.x;
            float dy = p1.y - p0.y;

            struct CutReq { int index; double cutBeat; };
            std::vector<CutReq> reqs;
            {
                auto& notes = noteSequence.getAllNotes();
                const int nBefore = (int) notes.size();
                for (int i = 0; i < nBefore; ++i)
                {
                    const NoteData& n = notes[(size_t) i];
                    auto bounds = boundsForNote(n);

                    float cutX;
                    if (std::abs(dx) < 0.5f)
                    {
                        cutX = p0.x;
                    }
                    else if (std::abs(dy) < 0.5f)
                    {
                        
                        cutX = p0.x + dx * 0.5f;
                    }
                    else
                    {
                        float targetY = bounds.getCentreY();
                        float t = (targetY - p0.y) / dy;
                        if (t < 0.0f || t > 1.0f) continue;
                        cutX = p0.x + t * dx;
                    }

                    if (cutX <= bounds.getX() + 1.0f || cutX >= bounds.getRight() - 1.0f)
                        continue;

                    double cutBeat = beatAtX(cutX);
                    const double minSliver = 0.02;
                    if (cutBeat <= n.startBeat + minSliver) continue;
                    if (cutBeat >= n.getEndBeat() - minSliver) continue;

                    reqs.push_back({ i, cutBeat });
                }
            }

            if (!reqs.empty())
            {
                saveUndoState();

                std::vector<std::pair<NoteData, NoteData>> halves;
                halves.reserve(reqs.size());
                {
                    auto& notes = noteSequence.getAllNotes();
                    for (auto& r : reqs)
                        halves.push_back(buildSplitPair(notes[(size_t) r.index], r.cutBeat));
                }

                std::sort(reqs.begin(), reqs.end(),
                          [](const CutReq& a, const CutReq& b){ return a.index > b.index; });
                for (auto& r : reqs)
                    noteSequence.removeNote(r.index);

                selectedNote = nullptr;
                clearMultiSelection();

                for (auto& h : halves)
                {
                    noteSequence.addNote(h.first);
                    noteSequence.addNote(h.second);
                }

                if (onNotesChanged) onNotesChanged();
            }
        }
        repaint();
    }

    if (dragMode == DragMode::MarqueeSelect)
    {
        if (dist <= dragThreshold && !deselectedOnDown)
        {
            
            saveUndoState();
            int noteNum = noteAtY(my);
            double snapBt = snapBeat(beatAtX(mx));
            NoteData newNote;
            newNote.noteNumber = noteNum;
            newNote.startBeat = snapBt;
            newNote.durationBeats = 1.0;
            newNote.velocity = 0.8f;
            noteSequence.addNote(newNote);
            selectedNote = &noteSequence.getAllNotes().back();
            clearMultiSelection();
            if (onNotesChanged) onNotesChanged();
            if (onNoteSelected) onNoteSelected(selectedNote);
        }
        else
        {
            
            marqueeActive = false;
        }
    }

    if (pointClickedOnDown && dist <= dragThreshold
        && clickedPointNote != nullptr && clickedPointIndex >= 0
        && clickedPointIndex < (int)clickedPointNote->pitchCurve.size())
    {
        saveUndoState();
        clickedPointNote->pitchCurve.erase(clickedPointNote->pitchCurve.begin() + clickedPointIndex);
        if (onNotesChanged) onNotesChanged();
    }

    if (curveDrawer.isDrawing())
    {
        auto* drawnNote = curveDrawer.getTargetNote();
        curveDrawer.endDrawing();

        if (drawnNote != nullptr && !drawnNote->pitchCurve.empty())
        {
            clearMultiSelection();
            selectedNote = drawnNote;
            for (int i = 0; i < (int)drawnNote->pitchCurve.size(); ++i)
                multiSelection.push_back({ drawnNote, i });
        }

        if (onNotesChanged) onNotesChanged();
    }

    dragMode = DragMode::None;
    undoSavedForDrag = false;
    duplicatedForDrag = false;
    draggedPointIndex = -1;
    pointClickedOnDown = false;
    clickedPointNote = nullptr;
    clickedPointIndex = -1;
    curvatureDragNote = nullptr;
    curvatureDragSegIndex = -1;
    vibratoHandleDragging = VibratoHandle::None;
    vibratoDragNote = nullptr;
    vibratoDragSegIndex = -1;
    activeScaleEdge = ScaleEdge::None;
    multiDragOrigPositions.clear();
    lineDragNote = nullptr;
    lineDragOrigPositions.clear();
    lineHoverMode = LineHoverMode::None;
    highlightEntireCurve = false;
    repaint();
}

void PianoRoll::updateMarqueeSelection()
{
    multiSelection.clear();
    if (marqueeRect.getWidth() < 2.0f && marqueeRect.getHeight() < 2.0f) return;

    for (auto& note : noteSequence.getAllNotes())
    {
        auto bounds = boundsForNote(note);

        bool anyDotInside = false;
        for (int i = 0; i < (int)note.pitchCurve.size(); ++i)
        {
            auto& pt = note.pitchCurve[(size_t)i];
            float px = bounds.getX() + (float)(pt.time / note.durationBeats) * bounds.getWidth();
            float py = bounds.getCentreY() - (float)pt.pitchOffset * getPixelsPerSemitone();

            if (marqueeRect.contains(px, py))
            {
                multiSelection.push_back({ &note, i });
                anyDotInside = true;
            }
        }

        if (!anyDotInside)
        {
            float handleY = bounds.getY();
            float handleCenterX = bounds.getCentreX();
            if (marqueeRect.contains(handleCenterX, handleY) ||
                marqueeRect.intersects(Rectangle<float>(bounds.getX(), bounds.getY(), bounds.getWidth(), handleHeight)))
            {
                multiSelection.push_back({ &note, -1 });
            }
        }
    }
}

void PianoRoll::mouseMove(const MouseEvent& e)
{
    float mx = (float)e.x;
    float my = (float)e.y;

    hoveredNote = findNoteAt(mx, my);
    currentHoverZone = HoverZone::None;
    handleHoveredNote = nullptr;
    hoveredSegmentNote = nullptr;
    hoveredSegmentIndex = -1;
    hoveredVibratoHandle = VibratoHandle::None;
    hoveredVibHandleNote = nullptr;
    hoveredVibHandleSeg = -1;
    lineHoverMode = LineHoverMode::None;
    highlightEntireCurve = false;

    hoveredPointIndex = -1;
    hoveredPointNote = nullptr;

    bool expressionActive = gamakaStampName.isNotEmpty();

    if (currentTool == EditorToolbar::Tool::Vibrato)
    {
        NoteData* vhNote = nullptr;
        int vhSeg = -1;
        VibratoHandle vh = findVibratoHandleAt(mx, my, vhNote, vhSeg);
        if (vh != VibratoHandle::None)
        {
            hoveredVibratoHandle = vh;
            hoveredVibHandleNote = vhNote;
            hoveredVibHandleSeg = vhSeg;

            if (vh == VibratoHandle::Waveform)
                setMouseCursor(MouseCursor::PointingHandCursor);
            else if (vh == VibratoHandle::FadeIn || vh == VibratoHandle::FadeOut)
                setMouseCursor(MouseCursor::LeftRightResizeCursor);
            else
                setMouseCursor(MouseCursor::UpDownResizeCursor);
            repaint();
            return;
        }
    }

    if (multiSelection.size() >= 2)
    {
        ScaleEdge edge = findScaleEdgeAt(mx, my);
        if (edge != hoveredScaleEdge)
        {
            hoveredScaleEdge = edge;
            repaint();
        }
        if (edge == ScaleEdge::Left || edge == ScaleEdge::Right)
        {
            setMouseCursor(MouseCursor::LeftRightResizeCursor);
            return;
        }
        else if (edge == ScaleEdge::Top || edge == ScaleEdge::Bottom)
        {
            setMouseCursor(MouseCursor::UpDownResizeCursor);
            return;
        }
    }
    else
    {
        if (hoveredScaleEdge != ScaleEdge::None)
        {
            hoveredScaleEdge = ScaleEdge::None;
            repaint();
        }
    }

    if (currentTool == EditorToolbar::Tool::Edit || currentTool == EditorToolbar::Tool::Pencil
        || currentTool == EditorToolbar::Tool::Vibrato)
    {
        
        NoteData* cpNote = nullptr;
        int cpIdx = -1;
        if (findAnyControlPoint(mx, my, cpNote, cpIdx))
        {
            hoveredPointIndex = cpIdx;
            hoveredPointNote = cpNote;
            currentHoverZone = HoverZone::ControlPoint;
            setMouseCursor(MouseCursor::PointingHandCursor);
            repaint();
            return;
        }

        {
            static constexpr float onLineThreshold = 10.0f;
            static constexpr float nearLineThreshold = 28.0f;

            NoteData* segNote = nullptr;
            int segIdx = -1;
            float segDist = 0.0f;
            if (findCurveSegmentWithDistance(mx, my, segNote, segIdx, segDist) && segDist < nearLineThreshold)
            {
                hoveredSegmentNote = segNote;

                if (e.mods.isAltDown())
                {
                    
                    hoveredSegmentIndex = segIdx;
                    lineHoverMode = LineHoverMode::None;
                    highlightEntireCurve = false;
                    setMouseCursor(MouseCursor::UpDownResizeCursor);
                }
                else if (segDist < onLineThreshold)
                {
                    
                    lineHoverMode = LineHoverMode::OnLine;
                    hoveredSegmentIndex = -1;  
                    highlightEntireCurve = false;

                    auto bounds = boundsForNote(*segNote);
                    float pps = getPixelsPerSemitone();
                    double relTime = (double)(mx - bounds.getX()) / (double)bounds.getWidth() * segNote->durationBeats;
                    relTime = jlimit(0.0, segNote->durationBeats, relTime);
                    float pitchAtMouse = PitchCurveInterpolator::interpolate(segNote->pitchCurve, relTime);
                    float dotY = bounds.getCentreY() - pitchAtMouse * pps;
                    previewDotPos = { mx, dotY };

                    setMouseCursor(MouseCursor::CrosshairCursor);
                }
                else
                {
                    
                    lineHoverMode = LineHoverMode::Near;
                    hoveredSegmentIndex = -1;
                    highlightEntireCurve = true;

                    setMouseCursor(MouseCursor::DraggingHandCursor);
                }
                repaint();
                return;
            }
            else
            {
                lineHoverMode = LineHoverMode::None;
                highlightEntireCurve = false;
            }
        }

        for (auto& note : noteSequence.getAllNotes())
        {
            auto bounds = boundsForNote(note);
            if (bounds.getRight() < 0 || bounds.getX() > getWidth())
                continue;

            if (isNearHandle(note, mx, my))
            {
                handleHoveredNote = &note;

                if (isInHandle(note, mx, my))
                {
                    float distFromLeft = mx - bounds.getX();
                    float distFromRight = bounds.getRight() - mx;

                    if (distFromLeft <= edgeThreshold)
                    {
                        currentHoverZone = HoverZone::HandleLeftEdge;
                        setMouseCursor(MouseCursor::LeftRightResizeCursor);
                    }
                    else if (distFromRight <= edgeThreshold)
                    {
                        currentHoverZone = HoverZone::HandleRightEdge;
                        setMouseCursor(MouseCursor::LeftRightResizeCursor);
                    }
                    else
                    {
                        currentHoverZone = HoverZone::Handle;
                        setMouseCursor(MouseCursor::DraggingHandCursor);
                    }
                }
                else
                {
                    
                    currentHoverZone = HoverZone::Body;
                    setMouseCursor(MouseCursor::CrosshairCursor);
                }
                repaint();
                return;
            }
        }

        if (hoveredNote != nullptr)
        {
            currentHoverZone = HoverZone::Body;
            setMouseCursor(MouseCursor::CrosshairCursor);
            repaint();
            return;
        }
    }

    if (expressionActive && hoveredNote != nullptr)
        setMouseCursor(expressionCursor);
    else if (expressionActive)
        setMouseCursor(MouseCursor::NormalCursor);
    else if (currentTool == EditorToolbar::Tool::Pencil || currentTool == EditorToolbar::Tool::Vibrato)
        setMouseCursor(MouseCursor::CrosshairCursor);
    else
        
        setMouseCursor(MouseCursor::NormalCursor);

    repaint();
}

void PianoRoll::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        
        double zoomFactor = 1.0 - wheel.deltaY * 0.5;
        double beatRange = viewEndBeat - viewStartBeat;
        double mousebeat = beatAtX((float)e.x);
        double leftPortion = (mousebeat - viewStartBeat) / beatRange;

        beatRange *= zoomFactor;
        beatRange = jlimit(1.0, 256.0, beatRange); 

        viewStartBeat = mousebeat - leftPortion * beatRange;
        viewEndBeat = viewStartBeat + beatRange;
    }
    else if (e.mods.isAltDown())
    {
        
        double range = viewHighest - viewLowest;
        if (range < 1.0) range = 1.0;

        double cursorPitch = viewHighest - ((double)e.y / (double)getHeight()) * range;

        double cursorFrac = (double)e.y / (double)getHeight();

        double zoomFactor = 1.0 - (double)wheel.deltaY * 0.4;
        double newRange = range * zoomFactor;
        newRange = jlimit(4.0, 96.0, newRange);

        viewHighest = cursorPitch + cursorFrac * newRange;
        viewLowest = viewHighest - newRange;
    }
    else
    {
        
        double noteRange = viewHighest - viewLowest;
        if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        {
            
            double shift = -wheel.deltaX * (viewEndBeat - viewStartBeat) * 0.1;
            viewStartBeat += shift;
            viewEndBeat += shift;
        }
        else
        {
            
            double shift = -(double)wheel.deltaY * noteRange * 0.2;
            viewLowest += shift;
            viewHighest += shift;
        }
    }

    if (onViewChanged) onViewChanged();
    repaint();
}

bool PianoRoll::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().startsWith("expression:");
}

void PianoRoll::itemDragMove(const SourceDetails& details)
{
    float mx = (float)details.localPosition.x;
    float my = (float)details.localPosition.y;
    NoteData* note = findNoteAt(mx, my);
    if (note != dropTargetNote)
    {
        dropTargetNote = note;
        repaint();
    }
}

void PianoRoll::itemDropped(const SourceDetails& details)
{
    auto desc = details.description.toString();
    if (!desc.startsWith("expression:")) return;

    String exprName = desc.fromFirstOccurrenceOf("expression:", false, false);
    float mx = (float)details.localPosition.x;
    float my = (float)details.localPosition.y;
    NoteData* note = findNoteAt(mx, my);

    if (note != nullptr && exprName.isNotEmpty())
    {
        saveUndoState();
        auto gamakaPoints = generateGamakaForNote(exprName, note, 1.0f);

        if (note->pitchCurve.empty())
            note->pitchCurve = gamakaPoints;
        else
            PitchCurveInterpolator::mergeIntoCurve(note->pitchCurve, gamakaPoints, 0.0);

        if (onNotesChanged) onNotesChanged();
    }

    dropTargetNote = nullptr;
    repaint();
}
