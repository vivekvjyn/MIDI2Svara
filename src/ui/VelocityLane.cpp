#include "VelocityLane.h"
#include <algorithm>
#include <set>


Rectangle<int> VelocityLane::tabBoundsForMode(LaneMode m) const
{
    if (leftMargin <= 0) return {};
    const int idx = (int)m;
    const int x = tabPadX;
    const int y = resizeGripHeight + tabPadY + idx * (tabHeightPx + 2);
    const int w = leftMargin - 2 * tabPadX;
    return { x, y, w, tabHeightPx };
}

VelocityLane::LaneMode VelocityLane::tabModeAtPoint(Point<int> p) const
{
    for (int m = 0; m <= (int)LaneMode::Dynamics; ++m)
    {
        auto r = tabBoundsForMode((LaneMode)m);
        if (r.contains(p)) return (LaneMode)m;
    }
    return laneMode;
}

void VelocityLane::paint(Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(FPColours::background.withAlpha(0.92f));
    g.fillRect(bounds);

    g.setColour(FPColours::textDim.withAlpha(0.3f));
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 2.0f);
    float cx = bounds.getCentreX();
    g.setColour(FPColours::textDim.withAlpha(0.5f));
    for (int i = -2; i <= 2; ++i)
        g.fillEllipse(cx + (float)i * 8.0f - 1.5f, 0.5f, 3.0f, 3.0f);

    float h = bounds.getHeight();
    float w = bounds.getWidth();
    float contentTop = (float)resizeGripHeight;
    float contentH = h - contentTop;
    if (contentH < 10.0f) return;

    {
        double beatRange = viewEndBeat - viewStartBeat;
        if (beatRange < 0.1) return;
        double gridStep = 1.0;
        if (beatRange > 64) gridStep = 4.0;
        else if (beatRange > 32) gridStep = 2.0;

        double firstBeat = std::floor(viewStartBeat / gridStep) * gridStep;
        for (double b = firstBeat; b <= viewEndBeat; b += gridStep)
        {
            float x = xForBeat(b);
            bool isBar = (std::fmod(b, 4.0) < 0.01);
            g.setColour(isBar ? FPColours::gridLineBright.withAlpha(0.4f)
                              : FPColours::gridLine.withAlpha(0.3f));
            g.drawVerticalLine((int)x, contentTop, h);
        }
    }

    float lm = (float)leftMargin;

    if (leftMargin > 0)
    {
        g.setColour(FPColours::background.withAlpha(0.95f));
        g.fillRect(0.0f, contentTop, lm, contentH);
        g.setColour(FPColours::gridLineBright.withAlpha(0.4f));
        g.drawVerticalLine((int)lm, contentTop, h);

        struct TabSpec { LaneMode mode; const char* label; };
        TabSpec tabs[] = {
            { LaneMode::Velocity, "Vel" },
            { LaneMode::Dynamics, "Dyn" }
        };
        g.setFont(FontOptions(9.5f, Font::bold));
        for (auto& t : tabs)
        {
            auto r = tabBoundsForMode(t.mode).toFloat();
            const bool active = (t.mode == laneMode);
            auto accent = (t.mode == LaneMode::Dynamics) ? FPColours::accentCyan
                                                          : FPColours::noteBlock;

            g.setColour(active ? accent.withAlpha(0.55f) : Colour(0x18ffffff));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(active ? accent.withAlpha(0.95f) : Colour(0x40ffffff));
            g.drawRoundedRectangle(r, 3.0f, 1.0f);
            g.setColour(active ? FPColours::text : FPColours::textDim);
            g.drawText(t.label, r, Justification::centred);
        }
    }

    struct RefLine { float frac; int midiVal; };
    RefLine refLines[] = { {1.0f, 127}, {0.75f, 95}, {0.5f, 64}, {0.25f, 32}, {0.008f, 1} };
    g.setFont(FontOptions(9.0f));
    float labelStartY = contentTop + (leftMargin > 0 ? (float)tabAreaHeightPx : 0.0f);
    for (auto& ref : refLines)
    {
        float y = contentTop + contentH * (1.0f - ref.frac);
        bool isMid = (ref.midiVal == 64);
        g.setColour(FPColours::gridLine.withAlpha(isMid ? 0.5f : 0.2f));
        g.drawHorizontalLine((int)y, lm, w);

        if (y >= labelStartY - 6.0f)
        {
            g.setColour(FPColours::textDim.withAlpha(0.6f));
            g.drawText(String(ref.midiVal), 2, (int)y - 6, (int)lm - 4, 12,
                       Justification::centredRight);
        }
    }

    g.saveState();
    g.reduceClipRegion(Rectangle<int>((int)lm, (int)contentTop,
                                             (int)(w - lm), (int)contentH));
    auto contentArea = Rectangle<float>(lm, contentTop, w - lm, contentH);

    if (laneMode == LaneMode::Velocity) paintVelocity(g, contentArea);
    else                                paintDynamics(g, contentArea);

    g.restoreState();

    g.setColour(FPColours::textDim.withAlpha(0.5f));
    g.setFont(FontOptions(10.0f));
    const char* label = (laneMode == LaneMode::Dynamics) ? "Dynamics" : "Velocity";
    g.drawText(label, (int)lm + 4, (int)contentTop + 2, 70, 12,
               Justification::centredLeft);

    if (laneMode == LaneMode::Dynamics && currentTool == EditorToolbar::Tool::Pencil)
    {
        g.setColour(FPColours::accentCyan.withAlpha(0.7f));
        g.setFont(FontOptions(9.0f));
        g.drawText("Pencil  (right-click drag = erase)",
                   Rectangle<int>((int)lm + 78, (int)contentTop + 2, 220, 12),
                   Justification::centredLeft);
    }
}

void VelocityLane::paintVelocity(Graphics& g, Rectangle<float> area)
{
    float lm = area.getX();
    float w  = area.getRight();
    float contentTop = area.getY();
    float contentH = area.getHeight();

    const std::vector<NoteData>& notes = isPreviewActive()
        ? previewNotes
        : noteSequence.getAllNotes();
    for (int i = 0; i < (int)notes.size(); ++i)
    {
        auto& note = notes[(size_t)i];
        float x1 = xForBeat(note.startBeat);
        float x2 = xForBeat(note.getEndBeat());
        if (x2 < lm || x1 > w) continue;

        float stemX = x1 + 1.0f;
        float stemTop = contentTop + contentH * (1.0f - note.velocity);
        float stemBot = contentTop + contentH;
        auto colour = note.selected ? FPColours::noteBlockSelected : FPColours::noteBlock;

        g.setColour(colour.withAlpha(0.45f));
        g.drawLine(stemX, stemTop, x2, stemTop, 1.0f);
        g.setColour(colour.withAlpha(0.8f));
        g.drawLine(stemX, stemTop, stemX, stemBot, 2.0f);
        g.setColour(colour);
        g.fillEllipse(stemX - 3.5f, stemTop - 3.5f, 7.0f, 7.0f);
        g.setColour(colour.withAlpha(0.2f));
        g.fillEllipse(stemX - 5.5f, stemTop - 5.5f, 11.0f, 11.0f);
    }
}

void VelocityLane::paintDynamics(Graphics& g, Rectangle<float> area)
{
    float lm = area.getX();
    float w  = area.getRight();
    float contentTop = area.getY();
    float contentH = area.getHeight();
    float contentBot = contentTop + contentH;

    const std::vector<NoteData>& notes = isPreviewActive()
        ? previewNotes
        : noteSequence.getAllNotes();
    const bool preview = isPreviewActive();
    auto cyan = FPColours::accentCyan;
    auto fillCol   = cyan.withAlpha(preview ? 0.22f : 0.18f);
    auto strokeCol = cyan.withAlpha(preview ? 0.95f : 0.85f);
    auto dotCol    = cyan.withAlpha(preview ? 0.7f : 1.0f);

    auto noteCurveX = [&](const NoteData& n, double tBeats) {
        return xForBeat(n.startBeat + tBeats);
    };

    for (auto& note : notes)
    {
        if (note.amplitudeCurve.empty()) continue;
        float nx1 = xForBeat(note.startBeat);
        float nx2 = xForBeat(note.getEndBeat());
        if (nx2 < lm || nx1 > w) continue;

        Path filled, stroked;
        bool started = false;
        for (auto& pt : note.amplitudeCurve)
        {
            float x = noteCurveX(note, pt.time);
            float y = yForValue(pt.value, contentTop, contentH);
            if (!started)
            {
                filled.startNewSubPath(x, contentBot);
                filled.lineTo(x, y);
                stroked.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                filled.lineTo(x, y);
                stroked.lineTo(x, y);
            }
        }
        if (started)
        {
            filled.lineTo(stroked.getCurrentPosition().x, contentBot);
            filled.closeSubPath();
            g.setColour(fillCol);
            g.fillPath(filled);
            g.setColour(strokeCol);
            g.strokePath(stroked, PathStrokeType(1.6f));
        }

        g.setColour(strokeCol.withAlpha(0.4f));
        g.drawVerticalLine((int)nx1, contentBot - 4.0f, contentBot);
    }

    for (int n = 0; n < (int)notes.size(); ++n)
    {
        auto& note = notes[(size_t)n];
        if (note.amplitudeCurve.empty()) continue;
        float nx1 = xForBeat(note.startBeat);
        float nx2 = xForBeat(note.getEndBeat());
        if (nx2 < lm || nx1 > w) continue;

        for (int p = 0; p < (int)note.amplitudeCurve.size(); ++p)
        {
            auto& pt = note.amplitudeCurve[(size_t)p];
            float x = noteCurveX(note, pt.time);
            float y = yForValue(pt.value, contentTop, contentH);

            const bool selected = !preview && isSelected(n, p);
            const bool hot = !preview
                          && ((n == hoverNote && p == hoverPoint)
                              || (n == dynDragNoteIdx && p == dynDragPointIdx));

            float r = (selected || hot) ? 5.0f : 3.5f;
            auto baseCol = selected ? Colour(0xffffd040) : dotCol;
            g.setColour(hot ? baseCol.brighter(0.4f) : baseCol);
            g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
            if (selected || hot)
            {
                g.setColour(baseCol.withAlpha(0.25f));
                g.fillEllipse(x - r - 3.0f, y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
            }
        }
    }

    if (marqueeActive && (marqueeRect.getWidth() > 1.0f || marqueeRect.getHeight() > 1.0f))
    {
        g.setColour(Colour(0x202090ff));
        g.fillRect(marqueeRect);
        g.setColour(Colour(0xa040b0ff));
        g.drawRect(marqueeRect, 1.0f);
    }

    if (!preview && dynSelection.size() >= 2)
    {
        auto sbRaw = selectionPixelBounds();
        if (!sbRaw.isEmpty())
        {
            auto sb = sbRaw.expanded(6.0f);
            g.setColour(Colour(0x80ffd040));
            float dashes[] = { 4.0f, 3.0f };
            g.drawDashedLine({ sb.getX(), sb.getY(), sb.getRight(), sb.getY() }, dashes, 2, 1.0f);
            g.drawDashedLine({ sb.getRight(), sb.getY(), sb.getRight(), sb.getBottom() }, dashes, 2, 1.0f);
            g.drawDashedLine({ sb.getRight(), sb.getBottom(), sb.getX(), sb.getBottom() }, dashes, 2, 1.0f);
            g.drawDashedLine({ sb.getX(), sb.getBottom(), sb.getX(), sb.getY() }, dashes, 2, 1.0f);

            for (auto eHandle : { EdgeHandle::Top, EdgeHandle::Bottom, EdgeHandle::Left, EdgeHandle::Right })
            {
                auto r = edgeHandleRect(eHandle);
                if (r.isEmpty()) continue;
                bool active = (dragEdge == eHandle);
                g.setColour(active ? Colour(0xffffe070) : Colour(0xffffd040));
                g.fillRoundedRectangle(r, 2.0f);
                if (active)
                {
                    g.setColour(Colour(0x40ffd040));
                    g.fillRoundedRectangle(r.expanded(3.0f), 3.0f);
                }
            }
        }
    }

    if ((dynDragMode == DynDragMode::ScaleHorizontal
         || dynDragMode == DynDragMode::ScaleVertical)
        && dragEdge != EdgeHandle::None)
    {
        String text;
        text << "x" << String(lastScaleFactor, 2);
        if (dynDragMode == DynDragMode::ScaleHorizontal) text << "  (time)";
        else                                              text << "  (amp)";
        g.setFont(FontOptions(11.0f, Font::bold));
        auto textW = 23; 
        Rectangle<int> badge((int)lastCursorPx.x + 12, (int)lastCursorPx.y - 22, textW, 18);
        g.setColour(Colour(0xee201818));
        g.fillRoundedRectangle(badge.toFloat(), 4.0f);
        g.setColour(Colour(0xffffd040));
        g.drawRoundedRectangle(badge.toFloat(), 4.0f, 1.0f);
        g.setColour(Colour(0xffffd040));
        g.drawText(text, badge, Justification::centred);
    }

    bool anyData = false;
    for (auto& n : notes) if (!n.amplitudeCurve.empty()) { anyData = true; break; }
    if (!anyData)
    {
        g.setColour(FPColours::textDim.withAlpha(0.55f));
        g.setFont(FontOptions(11.0f));
        g.drawText(preview ? "Building dynamics preview…"
                           : "No dynamics captured. Re-import audio with Dyn Smooth > 0,"
                             " or click within a note to draw.",
                   area.toNearestInt(), Justification::centred);
    }

    if (preview)
    {
        
        g.setColour(Colour(0xff80ffff).withAlpha(0.85f));
        g.setFont(FontOptions(9.5f, Font::bold));
        g.drawText("PREVIEW",
                   Rectangle<int>((int)area.getX() + 78, (int)area.getY() + 2, 70, 12),
                   Justification::centredLeft);
    }
}

int VelocityLane::noteIndexAtX(float x) const
{
    double beat = beatAtX(x);
    auto& notes = noteSequence.getAllNotes();
    int closest = -1;
    double closestDist = 1e30;
    for (int i = 0; i < (int)notes.size(); ++i)
    {
        auto& note = notes[(size_t)i];
        double mid = note.startBeat + note.durationBeats * 0.5;
        float nx1 = xForBeat(note.startBeat);
        float nx2 = xForBeat(note.getEndBeat());
        if (x >= nx1 - 6.0f && x <= nx2 + 6.0f)
        {
            double dist = std::abs(beat - mid);
            if (dist < closestDist) { closestDist = dist; closest = i; }
        }
    }
    return closest;
}

int VelocityLane::noteContainingBeat(double beat) const
{
    auto& notes = noteSequence.getAllNotes();
    for (int i = 0; i < (int)notes.size(); ++i)
    {
        auto& n = notes[(size_t)i];
        if (beat >= n.startBeat && beat <= n.getEndBeat()) return i;
    }
    return -1;
}

VelocityLane::AmpHit VelocityLane::dynamicsPointHitTest(Point<float> p) const
{
    constexpr float pickRadius = 7.0f;
    float contentTop = (float)resizeGripHeight;
    float contentH = (float)getHeight() - contentTop;

    auto& notes = noteSequence.getAllNotes();
    AmpHit best; float bestD2 = pickRadius * pickRadius;
    for (int n = 0; n < (int)notes.size(); ++n)
    {
        auto& note = notes[(size_t)n];
        for (int i = 0; i < (int)note.amplitudeCurve.size(); ++i)
        {
            auto& pt = note.amplitudeCurve[(size_t)i];
            float x = xForBeat(note.startBeat + pt.time);
            float y = yForValue(pt.value, contentTop, contentH);
            float d2 = (x - p.x) * (x - p.x) + (y - p.y) * (y - p.y);
            if (d2 <= bestD2) { bestD2 = d2; best = { n, i }; }
        }
    }
    return best;
}

bool VelocityLane::isSelected(int noteIdx, int pointIdx) const
{
    for (auto& s : dynSelection)
        if (s.noteIdx == noteIdx && s.pointIdx == pointIdx) return true;
    return false;
}

void VelocityLane::selectionAdd(int noteIdx, int pointIdx)
{
    if (isSelected(noteIdx, pointIdx)) return;
    dynSelection.push_back({ noteIdx, pointIdx });
}

void VelocityLane::selectionRemove(int noteIdx, int pointIdx)
{
    dynSelection.erase(std::remove_if(dynSelection.begin(), dynSelection.end(),
        [noteIdx, pointIdx](const DynItem& s) {
            return s.noteIdx == noteIdx && s.pointIdx == pointIdx;
        }), dynSelection.end());
}

void VelocityLane::selectionToggle(int noteIdx, int pointIdx)
{
    if (isSelected(noteIdx, pointIdx)) selectionRemove(noteIdx, pointIdx);
    else                                selectionAdd(noteIdx, pointIdx);
}

Rectangle<float> VelocityLane::selectionPixelBounds() const
{
    if (dynSelection.empty()) return {};
    float contentTop = (float)resizeGripHeight;
    float contentH = (float)getHeight() - contentTop;
    auto& notes = noteSequence.getAllNotes();

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& s : dynSelection)
    {
        if (s.noteIdx < 0 || s.noteIdx >= (int)notes.size()) continue;
        auto& note = notes[(size_t)s.noteIdx];
        if (s.pointIdx < 0 || s.pointIdx >= (int)note.amplitudeCurve.size()) continue;
        auto& pt = note.amplitudeCurve[(size_t)s.pointIdx];
        float x = xForBeat(note.startBeat + pt.time);
        float y = yForValue(pt.value, contentTop, contentH);
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
    if (minX > maxX) return {};
    return { minX, minY, maxX - minX, maxY - minY };
}

Rectangle<float> VelocityLane::edgeHandleRect(EdgeHandle e) const
{
    if (dynSelection.size() < 2) return {};
    auto sb = selectionPixelBounds();
    if (sb.isEmpty()) return {};
    sb = sb.expanded(6.0f);
    constexpr float w = 12.0f, h = 10.0f;
    switch (e)
    {
        case EdgeHandle::Top:
            return { sb.getCentreX() - w * 0.5f, sb.getY()       - h * 0.5f, w, h };
        case EdgeHandle::Bottom:
            return { sb.getCentreX() - w * 0.5f, sb.getBottom()  - h * 0.5f, w, h };
        case EdgeHandle::Left:
            return { sb.getX()       - w * 0.5f, sb.getCentreY() - h * 0.5f, w, h };
        case EdgeHandle::Right:
            return { sb.getRight()   - w * 0.5f, sb.getCentreY() - h * 0.5f, w, h };
        case EdgeHandle::None:
            return {};
    }
    return {};
}

VelocityLane::EdgeHandle VelocityLane::edgeHandleAtPoint(Point<float> p) const
{
    if (dynSelection.size() < 2) return EdgeHandle::None;
    for (auto e : { EdgeHandle::Top, EdgeHandle::Bottom, EdgeHandle::Left, EdgeHandle::Right })
        if (edgeHandleRect(e).contains(p)) return e;
    return EdgeHandle::None;
}

int VelocityLane::addAmplitudePoint(int noteIdx, double absoluteBeat, float value)
{
    auto& notes = noteSequence.getAllNotes();
    if (noteIdx < 0 || noteIdx >= (int)notes.size()) return -1;
    auto& note = notes[(size_t)noteIdx];
    AmplitudePoint p;
    p.time  = jlimit(0.0, note.durationBeats, absoluteBeat - note.startBeat);
    p.value = jlimit(0.0f, 1.0f, value);
    auto& curve = note.amplitudeCurve;
    auto it = std::lower_bound(curve.begin(), curve.end(), p,
        [](const AmplitudePoint& a, const AmplitudePoint& b) { return a.time < b.time; });
    int idx = (int)std::distance(curve.begin(), curve.insert(it, p));
    return idx;
}

int VelocityLane::moveAmplitudePoint(int noteIdx, int pointIdx, double absoluteBeat, float value)
{
    auto& notes = noteSequence.getAllNotes();
    if (noteIdx < 0 || noteIdx >= (int)notes.size()) return -1;
    auto& note = notes[(size_t)noteIdx];
    auto& curve = note.amplitudeCurve;
    if (pointIdx < 0 || pointIdx >= (int)curve.size()) return -1;

    curve[(size_t)pointIdx].time  = jlimit(0.0, note.durationBeats, absoluteBeat - note.startBeat);
    curve[(size_t)pointIdx].value = jlimit(0.0f, 1.0f, value);

    AmplitudePoint moved = curve[(size_t)pointIdx];
    std::stable_sort(curve.begin(), curve.end(),
        [](const AmplitudePoint& a, const AmplitudePoint& b) { return a.time < b.time; });

    for (int i = 0; i < (int)curve.size(); ++i)
        if (std::abs(curve[(size_t)i].time - moved.time) < 1e-9
            && std::abs(curve[(size_t)i].value - moved.value) < 1e-6)
            return i;
    return pointIdx;
}

void VelocityLane::deleteAmplitudePoint(int noteIdx, int pointIdx)
{
    auto& notes = noteSequence.getAllNotes();
    if (noteIdx < 0 || noteIdx >= (int)notes.size()) return;
    auto& curve = notes[(size_t)noteIdx].amplitudeCurve;
    if (pointIdx < 0 || pointIdx >= (int)curve.size()) return;
    curve.erase(curve.begin() + pointIdx);
}

void VelocityLane::eraseAmplitudePointsAt(Point<float> p)
{
    constexpr float pickRadius = 8.0f;
    float contentTop = (float)resizeGripHeight;
    float contentH = (float)getHeight() - contentTop;
    auto& notes = noteSequence.getAllNotes();
    for (int n = 0; n < (int)notes.size(); ++n)
    {
        auto& note = notes[(size_t)n];
        auto& c = note.amplitudeCurve;
        for (int i = (int)c.size() - 1; i >= 0; --i)
        {
            float x = xForBeat(note.startBeat + c[(size_t)i].time);
            float y = yForValue(c[(size_t)i].value, contentTop, contentH);
            if (std::abs(x - p.x) <= pickRadius && std::abs(y - p.y) <= pickRadius)
            {
                c.erase(c.begin() + i);
                selectionRemove(n, i);
            }
        }
    }
    clearDynamicsHover();
}

void VelocityLane::setVelocityAtMouse(const MouseEvent& e)
{
    float contentTop = (float)resizeGripHeight;
    float contentH = (float)getHeight() - contentTop;
    if (contentH < 1.0f) return;
    int idx = noteIndexAtX((float)e.x);
    if (idx < 0) return;
    float vel = 1.0f - jlimit(0.0f, 1.0f, ((float)e.y - contentTop) / contentH);
    vel = jlimit(0.01f, 1.0f, vel);
    noteSequence.getNote(idx).velocity = vel;
    notifyChanged();
}

void VelocityLane::mouseDown(const MouseEvent& e)
{
    grabKeyboardFocus();
    noteFocused();

    if (isInResizeGrip(e.y))
    {
        resizeDragging = true;
        resizeDragStartY = e.getScreenY();
        resizeDragStartHeight = desiredHeight;
        return;
    }

    if (e.x < leftMargin)
    {
        LaneMode tab = tabModeAtPoint(e.getPosition());
        if (tab != laneMode) { setLaneMode(tab); return; }
    }

    if (isPreviewActive()) return;

    if (laneMode == LaneMode::Velocity)
    {
        if (e.mods.isLeftButtonDown())
        {
            if (onUndoNeeded) onUndoNeeded();
            dragging = true;
            setVelocityAtMouse(e);
        }
        return;
    }

    const float contentTop = (float)resizeGripHeight;
    const float contentH = (float)getHeight() - contentTop;
    const auto pos = e.position;
    dragStartPx = pos;

    if (e.mods.isRightButtonDown())
    {
        if (onUndoNeeded) onUndoNeeded();
        dynDragMode = DynDragMode::Erase;
        eraseAmplitudePointsAt(pos);
        notifyChanged();
        return;
    }

    if (!e.mods.isLeftButtonDown()) return;

    if (currentTool == EditorToolbar::Tool::Pencil)
    {
        if (onUndoNeeded) onUndoNeeded();
        double beat = beatAtX(pos.x);
        int n = noteContainingBeat(beat);
        if (n < 0) return;
        float value = valueAtY(pos.y, contentTop, contentH);
        int newIdx = addAmplitudePoint(n, beat, value);
        dynDragMode = DynDragMode::Draw;
        dynDragNoteIdx = n;
        dynDragPointIdx = newIdx;
        selectionClear();
        notifyChanged();
        return;
    }

    auto edge = edgeHandleAtPoint(pos);
    if (edge != EdgeHandle::None)
    {
        if (onUndoNeeded) onUndoNeeded();
        dragEdge = edge;
        dynDragMode = (edge == EdgeHandle::Top || edge == EdgeHandle::Bottom)
                       ? DynDragMode::ScaleVertical
                       : DynDragMode::ScaleHorizontal;
        lastScaleFactor = 1.0f;
        multiDragSnap.clear();
        auto& notes = noteSequence.getAllNotes();
        for (auto& s : dynSelection)
        {
            if (s.noteIdx < 0 || s.noteIdx >= (int)notes.size()) continue;
            auto& c = notes[(size_t)s.noteIdx].amplitudeCurve;
            if (s.pointIdx < 0 || s.pointIdx >= (int)c.size()) continue;
            multiDragSnap.push_back({ s.noteIdx, s.pointIdx,
                                       c[(size_t)s.pointIdx].time,
                                       c[(size_t)s.pointIdx].value });
        }
        multiDragOrigBounds = selectionPixelBounds();
        return;
    }

    auto hit = dynamicsPointHitTest(pos);
    if (hit.noteIdx >= 0)
    {
        
        if (e.mods.isCommandDown())
        {
            selectionToggle(hit.noteIdx, hit.pointIdx);
            repaint();
            return; 
        }
        if (e.mods.isShiftDown())
        {
            selectionAdd(hit.noteIdx, hit.pointIdx);
            
        }
        else
        {
            
            if (!isSelected(hit.noteIdx, hit.pointIdx))
            {
                selectionClear();
                selectionAdd(hit.noteIdx, hit.pointIdx);
            }
        }

        if (onUndoNeeded) onUndoNeeded();

        if (dynSelection.size() > 1)
        {
            
            dynDragMode = DynDragMode::MarqueeMove;
            dynDragNoteIdx = hit.noteIdx;
            dynDragPointIdx = hit.pointIdx;
            dragAxisLock = AxisLock::None;
            multiDragSnap.clear();
            auto& notes = noteSequence.getAllNotes();
            for (auto& s : dynSelection)
            {
                if (s.noteIdx < 0 || s.noteIdx >= (int)notes.size()) continue;
                auto& c = notes[(size_t)s.noteIdx].amplitudeCurve;
                if (s.pointIdx < 0 || s.pointIdx >= (int)c.size()) continue;
                multiDragSnap.push_back({ s.noteIdx, s.pointIdx,
                                          c[(size_t)s.pointIdx].time,
                                          c[(size_t)s.pointIdx].value });
            }
        }
        else
        {
            dynDragMode = DynDragMode::DragPoint;
            dynDragNoteIdx = hit.noteIdx;
            dynDragPointIdx = hit.pointIdx;
        }
        repaint();
        return;
    }

    if (dynSelection.size() >= 2)
    {
        auto sb = selectionPixelBounds().expanded(8.0f);
        if (!sb.isEmpty() && sb.contains(pos))
        {
            if (onUndoNeeded) onUndoNeeded();
            dynDragMode = DynDragMode::MarqueeMove;
            dragAxisLock = AxisLock::None;
            multiDragSnap.clear();
            auto& notes = noteSequence.getAllNotes();
            for (auto& s : dynSelection)
            {
                if (s.noteIdx < 0 || s.noteIdx >= (int)notes.size()) continue;
                auto& c = notes[(size_t)s.noteIdx].amplitudeCurve;
                if (s.pointIdx < 0 || s.pointIdx >= (int)c.size()) continue;
                multiDragSnap.push_back({ s.noteIdx, s.pointIdx,
                                          c[(size_t)s.pointIdx].time,
                                          c[(size_t)s.pointIdx].value });
            }
            return;
        }
    }

    double beat = beatAtX(pos.x);
    int n = noteContainingBeat(beat);
    if (n < 0)
    {
        if (!e.mods.isShiftDown() && !e.mods.isCommandDown())
            selectionClear();
        marqueeRect = { pos.x, pos.y, 0.0f, 0.0f };
        marqueeActive = true;
        dynDragMode = DynDragMode::MarqueeSelect;
        return;
    }

    if (onUndoNeeded) onUndoNeeded();
    float value = valueAtY(pos.y, contentTop, contentH);
    int newIdx = addAmplitudePoint(n, beat, value);
    dynDragMode = DynDragMode::Draw;
    dynDragNoteIdx = n;
    dynDragPointIdx = newIdx;
    selectionClear();
    notifyChanged();
}

void VelocityLane::mouseDrag(const MouseEvent& e)
{
    if (resizeDragging)
    {
        int delta = resizeDragStartY - e.getScreenY();
        int rawH = resizeDragStartHeight + delta;
        if (rawH < minHeight - 10) { resizeDragging = false; if (onAutoClosed) onAutoClosed(); return; }
        int newH = jlimit(minHeight, 300, rawH);
        desiredHeight = newH;
        if (onResized) onResized(newH);
        return;
    }

    if (laneMode == LaneMode::Velocity)
    {
        if (dragging) setVelocityAtMouse(e);
        return;
    }

    if (laneMode != LaneMode::Dynamics) return;

    const float contentTop = (float)resizeGripHeight;
    const float contentH = (float)getHeight() - contentTop;
    const auto pos = e.position;

    switch (dynDragMode)
    {
        case DynDragMode::Erase:
            eraseAmplitudePointsAt(pos);
            notifyChanged();
            return;

        case DynDragMode::DragPoint:
        {
            float value = valueAtY(pos.y, contentTop, contentH);
            double beat = beatAtX(pos.x);
            dynDragPointIdx = moveAmplitudePoint(dynDragNoteIdx, dynDragPointIdx, beat, value);
            
            if (dynSelection.size() == 1)
            {
                dynSelection[0].noteIdx  = dynDragNoteIdx;
                dynSelection[0].pointIdx = dynDragPointIdx;
            }
            notifyChanged();
            return;
        }

        case DynDragMode::Draw:
        {
            if (dynDragNoteIdx < 0) return;
            auto& curve = noteSequence.getNote(dynDragNoteIdx).amplitudeCurve;
            if (curve.empty()) return;
            const auto& note = noteSequence.getNote(dynDragNoteIdx);
            double beat = beatAtX(pos.x);
            double localT = jlimit(0.0, note.durationBeats, beat - note.startBeat);
            float value = valueAtY(pos.y, contentTop, contentH);

            auto& last = curve[(size_t)dynDragPointIdx];
            if (std::abs(localT - last.time) < 0.025)
            {
                last.value = jlimit(0.0f, 1.0f, value);
            }
            else
            {
                int newIdx = addAmplitudePoint(dynDragNoteIdx, beat, value);
                dynDragPointIdx = newIdx;
            }
            notifyChanged();
            return;
        }

        case DynDragMode::MarqueeSelect:
        {
            float x0 = std::min(dragStartPx.x, pos.x);
            float y0 = std::min(dragStartPx.y, pos.y);
            float x1 = std::max(dragStartPx.x, pos.x);
            float y1 = std::max(dragStartPx.y, pos.y);
            marqueeRect = { x0, y0, x1 - x0, y1 - y0 };

            auto& notes = noteSequence.getAllNotes();
            if (!e.mods.isShiftDown() && !e.mods.isCommandDown())
                selectionClear();
            for (int n = 0; n < (int)notes.size(); ++n)
            {
                auto& note = notes[(size_t)n];
                for (int i = 0; i < (int)note.amplitudeCurve.size(); ++i)
                {
                    auto& pt = note.amplitudeCurve[(size_t)i];
                    float x = xForBeat(note.startBeat + pt.time);
                    float y = yForValue(pt.value, contentTop, contentH);
                    if (marqueeRect.contains(x, y))
                        selectionAdd(n, i);
                }
            }
            repaint();
            return;
        }

        case DynDragMode::MarqueeMove:
        {
            if (multiDragSnap.empty()) return;

            float pxDx = pos.x - dragStartPx.x;
            float pxDy = pos.y - dragStartPx.y;

            if (dragAxisLock == AxisLock::None && !e.mods.isShiftDown())
            {
                if (std::max(std::abs(pxDx), std::abs(pxDy)) > axisLockThresholdPx)
                {
                    dragAxisLock = (std::abs(pxDx) >= std::abs(pxDy))
                                    ? AxisLock::Horizontal
                                    : AxisLock::Vertical;
                }
            }
            
            if (e.mods.isShiftDown()) dragAxisLock = AxisLock::None;

            float effPxDx = pxDx;
            float effPxDy = pxDy;
            if (dragAxisLock == AxisLock::Horizontal) effPxDy = 0.0f;
            else if (dragAxisLock == AxisLock::Vertical)   effPxDx = 0.0f;

            double dBeat  = beatAtX(dragStartPx.x + effPxDx) - beatAtX(dragStartPx.x);
            float  dValue = valueAtY(dragStartPx.y + effPxDy, contentTop, contentH)
                          - valueAtY(dragStartPx.y, contentTop, contentH);

            auto& notes = noteSequence.getAllNotes();
            for (auto& snap : multiDragSnap)
            {
                if (snap.noteIdx < 0 || snap.noteIdx >= (int)notes.size()) continue;
                auto& note = notes[(size_t)snap.noteIdx];
                if (snap.pointIdx < 0 || snap.pointIdx >= (int)note.amplitudeCurve.size()) continue;

                double newAbsBeat = note.startBeat + snap.timeAtNoteStart + dBeat;
                float  newValue   = jlimit(0.0f, 1.0f, snap.value + dValue);
                snap.pointIdx = moveAmplitudePoint(snap.noteIdx, snap.pointIdx, newAbsBeat, newValue);
            }
            
            dynSelection.clear();
            for (auto& s : multiDragSnap) dynSelection.push_back({ s.noteIdx, s.pointIdx });
            notifyChanged();
            return;
        }

        case DynDragMode::ScaleHorizontal:
        {
            if (multiDragSnap.empty() || multiDragOrigBounds.isEmpty()) return;
            lastCursorPx = pos;

            float anchorPx;
            if (e.mods.isAltDown())
                anchorPx = multiDragOrigBounds.getCentreX();
            else if (dragEdge == EdgeHandle::Right)
                anchorPx = multiDragOrigBounds.getX();      
            else 
                anchorPx = multiDragOrigBounds.getRight();  

            float draggedEdgePx0 = (dragEdge == EdgeHandle::Right)
                                   ? multiDragOrigBounds.getRight()
                                   : multiDragOrigBounds.getX();

            double anchorBeat   = beatAtX(anchorPx);
            double draggedBeat0 = beatAtX(draggedEdgePx0);
            double draggedBeatN = beatAtX(pos.x);
            double origSpan = draggedBeat0 - anchorBeat;
            double newSpan  = draggedBeatN - anchorBeat;
            if (std::abs(origSpan) < 1e-6) return;
            double scale = newSpan / origSpan;
            scale = jlimit(0.05, 8.0, scale);
            lastScaleFactor = (float)scale;

            auto& notes = noteSequence.getAllNotes();
            for (auto& snap : multiDragSnap)
            {
                if (snap.noteIdx < 0 || snap.noteIdx >= (int)notes.size()) continue;
                auto& note = notes[(size_t)snap.noteIdx];
                if (snap.pointIdx < 0 || snap.pointIdx >= (int)note.amplitudeCurve.size()) continue;
                double origAbs = note.startBeat + snap.timeAtNoteStart;
                double newAbs  = anchorBeat + (origAbs - anchorBeat) * scale;
                snap.pointIdx = moveAmplitudePoint(snap.noteIdx, snap.pointIdx, newAbs, snap.value);
            }
            dynSelection.clear();
            for (auto& s : multiDragSnap) dynSelection.push_back({ s.noteIdx, s.pointIdx });
            notifyChanged();
            return;
        }

        case DynDragMode::ScaleVertical:
        {
            if (multiDragSnap.empty() || multiDragOrigBounds.isEmpty()) return;
            lastCursorPx = pos;

            float anchorVal;
            if (e.mods.isAltDown())
                anchorVal = valueAtY(multiDragOrigBounds.getCentreY(), contentTop, contentH);
            else if (dragEdge == EdgeHandle::Top)
                anchorVal = valueAtY(multiDragOrigBounds.getBottom(),   contentTop, contentH);
            else 
                anchorVal = valueAtY(multiDragOrigBounds.getY(),         contentTop, contentH);

            float draggedEdgeY0 = (dragEdge == EdgeHandle::Top)
                                   ? multiDragOrigBounds.getY()
                                   : multiDragOrigBounds.getBottom();
            float draggedVal0 = valueAtY(draggedEdgeY0, contentTop, contentH);
            float draggedValN = valueAtY(pos.y,         contentTop, contentH);

            float origSpan = draggedVal0 - anchorVal;
            float newSpan  = draggedValN - anchorVal;
            if (std::abs(origSpan) < 1e-4) return;
            float scale = newSpan / origSpan;
            scale = jlimit(-2.0f, 4.0f, scale);
            lastScaleFactor = scale;

            auto& notes = noteSequence.getAllNotes();
            for (auto& snap : multiDragSnap)
            {
                if (snap.noteIdx < 0 || snap.noteIdx >= (int)notes.size()) continue;
                auto& note = notes[(size_t)snap.noteIdx];
                if (snap.pointIdx < 0 || snap.pointIdx >= (int)note.amplitudeCurve.size()) continue;
                double origAbs = note.startBeat + snap.timeAtNoteStart;
                float  newVal  = jlimit(0.0f, 1.0f, anchorVal + (snap.value - anchorVal) * scale);
                snap.pointIdx  = moveAmplitudePoint(snap.noteIdx, snap.pointIdx, origAbs, newVal);
            }
            dynSelection.clear();
            for (auto& s : multiDragSnap) dynSelection.push_back({ s.noteIdx, s.pointIdx });
            notifyChanged();
            return;
        }

        case DynDragMode::None:
            return;
    }
}

void VelocityLane::mouseUp(const MouseEvent&)
{
    dragging = false;
    resizeDragging = false;
    if (dynDragMode == DynDragMode::MarqueeSelect)
    {
        marqueeActive = false;
        marqueeRect = {};
    }
    dynDragMode = DynDragMode::None;
    dynDragNoteIdx = -1;
    dynDragPointIdx = -1;
    dragAxisLock = AxisLock::None;
    dragEdge = EdgeHandle::None;
    lastScaleFactor = 1.0f;
    multiDragSnap.clear();
    multiDragOrigBounds = {};
    repaint();
}

void VelocityLane::mouseMove(const MouseEvent& e)
{
    if (isInResizeGrip(e.y))
    {
        setMouseCursor(MouseCursor::UpDownResizeCursor);
        setTooltip({});
        return;
    }

    if (laneMode == LaneMode::Velocity)
    {
        setMouseCursor(MouseCursor::CrosshairCursor);
        int idx = noteIndexAtX((float)e.x);
        if (idx >= 0)
        {
            auto& note = noteSequence.getNote(idx);
            setTooltip("Velocity: " + String((int)(note.velocity * 127.0f)));
        }
        else setTooltip({});
        return;
    }

    auto edge = edgeHandleAtPoint(e.position);
    if (edge != EdgeHandle::None)
    {
        if (edge == EdgeHandle::Top || edge == EdgeHandle::Bottom)
        {
            setMouseCursor(MouseCursor::UpDownResizeCursor);
            setTooltip(String((edge == EdgeHandle::Top) ? "Top: " : "Bottom: ")
                       + "drag to scale amplitude (Alt = scale around centre)");
        }
        else
        {
            setMouseCursor(MouseCursor::LeftRightResizeCursor);
            setTooltip(String((edge == EdgeHandle::Left) ? "Left: " : "Right: ")
                       + "drag to scale time (Alt = scale around centre)");
        }
        return;
    }

    auto hit = dynamicsPointHitTest(e.position);
    if (hit.noteIdx != hoverNote || hit.pointIdx != hoverPoint)
    {
        hoverNote = hit.noteIdx; hoverPoint = hit.pointIdx;
        repaint();
    }

    if (hit.noteIdx >= 0)
    {
        auto& pt = noteSequence.getNote(hit.noteIdx).amplitudeCurve[(size_t)hit.pointIdx];
        setMouseCursor(MouseCursor::PointingHandCursor);
        setTooltip("Dyn: " + String((int)(pt.value * 127.0f))
                   + "   (Shift/Cmd+click multi-select, right-click delete)");
        return;
    }

    if (dynSelection.size() >= 2)
    {
        auto sb = selectionPixelBounds().expanded(8.0f);
        if (!sb.isEmpty() && sb.contains(e.position))
        {
            setMouseCursor(MouseCursor::DraggingHandCursor);
            setTooltip("Drag to move selection (axis follows cursor; hold Shift for free move)");
            return;
        }
    }

    setMouseCursor(currentTool == EditorToolbar::Tool::Pencil
                   ? MouseCursor::CrosshairCursor
                   : MouseCursor::NormalCursor);
    setTooltip(currentTool == EditorToolbar::Tool::Pencil
               ? "Pencil: drag to draw, right-drag to erase"
               : "Drag in note to add. Drag empty space for marquee.");
}

void VelocityLane::mouseExit(const MouseEvent&)
{
    if (hoverNote >= 0 || hoverPoint >= 0) { clearDynamicsHover(); repaint(); }
}

void VelocityLane::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (onMouseWheel) onMouseWheel(e, wheel);
}

bool VelocityLane::keyPressed(const KeyPress& key)
{
    if (laneMode != LaneMode::Dynamics) return false;

    if (key == KeyPress('a', ModifierKeys::commandModifier, 0))
        return selectAllDynamics();

    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
        return deleteSelectedDynamics();

    return false;
}

VelocityLane::NoteToPoints VelocityLane::groupSelectionByNote() const
{
    NoteToPoints out;
    auto& notes = noteSequence.getAllNotes();

    if (dynSelection.empty())
    {
        
        for (int n = 0; n < (int)notes.size(); ++n)
        {
            if (notes[(size_t)n].amplitudeCurve.empty()) continue;
            std::vector<int> indices;
            indices.reserve(notes[(size_t)n].amplitudeCurve.size());
            for (int i = 0; i < (int)notes[(size_t)n].amplitudeCurve.size(); ++i)
                indices.push_back(i);
            out.emplace_back(n, std::move(indices));
        }
        return out;
    }

    std::map<int, std::vector<int>> grouped;
    for (auto& s : dynSelection) grouped[s.noteIdx].push_back(s.pointIdx);
    for (auto& kv : grouped)
    {
        std::sort(kv.second.begin(), kv.second.end());
        kv.second.erase(std::unique(kv.second.begin(), kv.second.end()), kv.second.end());
        out.emplace_back(kv.first, std::move(kv.second));
    }
    return out;
}

bool VelocityLane::selectAllDynamics()
{
    selectionClear();
    auto& notes = noteSequence.getAllNotes();
    for (int n = 0; n < (int)notes.size(); ++n)
        for (int i = 0; i < (int)notes[(size_t)n].amplitudeCurve.size(); ++i)
            selectionAdd(n, i);
    repaint();
    return true;
}

bool VelocityLane::deleteSelectedDynamics()
{
    if (dynSelection.empty()) return false;
    if (onUndoNeeded) onUndoNeeded();

    auto groups = groupSelectionByNote();
    auto& notes = noteSequence.getAllNotes();
    for (auto& [noteIdx, points] : groups)
    {
        if (noteIdx < 0 || noteIdx >= (int)notes.size()) continue;
        auto& curve = notes[(size_t)noteIdx].amplitudeCurve;
        for (auto it = points.rbegin(); it != points.rend(); ++it)
            if (*it >= 0 && *it < (int)curve.size())
                curve.erase(curve.begin() + *it);
    }
    selectionClear();
    notifyChanged();
    return true;
}

static void simplifyDP(const std::vector<AmplitudePoint>& input,
                        std::vector<bool>& keep,
                        int first, int last, float valueTol, double timeTol)
{
    if (last <= first + 1) return;
    const auto& a = input[(size_t)first];
    const auto& b = input[(size_t)last];
    double dt = b.time - a.time;
    float  dv = b.value - a.value;
    double len2 = dt * dt + dv * dv;

    int   maxIdx = -1;
    float maxDist = 0.0f;
    for (int i = first + 1; i < last; ++i)
    {
        const auto& p = input[(size_t)i];
        
        float dist;
        if (len2 < 1e-12)
        {
            float d1 = p.value - a.value;
            double d2 = p.time - a.time;
            dist = std::sqrt((float)(d1 * d1 + d2 * d2));
        }
        else
        {
            double t = ((p.time - a.time) * dt + (p.value - a.value) * dv) / len2;
            t = jlimit(0.0, 1.0, t);
            double cx = a.time  + t * dt;
            float  cy = a.value + (float)t * dv;
            double dx = p.time - cx;
            float  dy = p.value - cy;
            dist = std::sqrt((float)(dx * dx + dy * dy));
        }
        if (dist > maxDist) { maxDist = dist; maxIdx = i; }
    }

    float tol = std::max(valueTol, (float)timeTol);
    if (maxDist > tol && maxIdx >= 0)
    {
        keep[(size_t)maxIdx] = true;
        simplifyDP(input, keep, first, maxIdx, valueTol, timeTol);
        simplifyDP(input, keep, maxIdx, last, valueTol, timeTol);
    }
}

void VelocityLane::simplifyDynamicsSelection()
{
    if (laneMode != LaneMode::Dynamics) return;
    if (onUndoNeeded) onUndoNeeded();

    auto groups = groupSelectionByNote();
    auto& notes = noteSequence.getAllNotes();
    constexpr float kValueTol = 0.025f;  
    constexpr double kTimeTol = 0.01;     

    for (auto& [noteIdx, points] : groups)
    {
        if (noteIdx < 0 || noteIdx >= (int)notes.size()) continue;
        auto& curve = notes[(size_t)noteIdx].amplitudeCurve;
        if ((int)curve.size() < 3) continue;

        std::vector<int> sortedIdx = points;
        std::sort(sortedIdx.begin(), sortedIdx.end());
        
        std::vector<std::pair<int, int>> runs;
        int runStart = sortedIdx.empty() ? -1 : sortedIdx.front();
        int prev = runStart;
        for (size_t k = 1; k < sortedIdx.size(); ++k)
        {
            if (sortedIdx[k] == prev + 1) { prev = sortedIdx[k]; continue; }
            runs.emplace_back(runStart, prev);
            runStart = sortedIdx[k]; prev = runStart;
        }
        if (runStart >= 0) runs.emplace_back(runStart, prev);

        std::vector<bool> keep(curve.size(), false);
        
        std::set<int> selectedSet(sortedIdx.begin(), sortedIdx.end());
        for (int i = 0; i < (int)curve.size(); ++i)
            if (selectedSet.count(i) == 0) keep[(size_t)i] = true;

        for (auto& [s, e] : runs)
        {
            if (e - s < 2) { for (int i = s; i <= e; ++i) keep[(size_t)i] = true; continue; }
            keep[(size_t)s] = true;
            keep[(size_t)e] = true;
            simplifyDP(curve, keep, s, e, kValueTol, kTimeTol);
        }

        std::vector<AmplitudePoint> rebuilt;
        rebuilt.reserve(curve.size());
        for (int i = 0; i < (int)curve.size(); ++i)
            if (keep[(size_t)i]) rebuilt.push_back(curve[(size_t)i]);
        curve = std::move(rebuilt);
    }
    selectionClear();
    notifyChanged();
}

static void smoothRun(std::vector<AmplitudePoint>& curve, int first, int last,
                      float intensity)
{
    if (last - first < 2) return;
    float clamped = jlimit(0.0f, 1.0f, intensity);
    if (clamped <= 0.0f) return;
    float alpha = 0.95f * std::pow(clamped, 0.7f);
    
    for (int i = first + 1; i <= last; ++i)
        curve[(size_t)i].value = alpha * curve[(size_t)(i - 1)].value
                                + (1.0f - alpha) * curve[(size_t)i].value;
    
    for (int i = last - 1; i >= first; --i)
        curve[(size_t)i].value = alpha * curve[(size_t)(i + 1)].value
                                + (1.0f - alpha) * curve[(size_t)i].value;
}

void VelocityLane::smoothDynamicsSelection(float intensity)
{
    if (laneMode != LaneMode::Dynamics) return;
    if (onUndoNeeded) onUndoNeeded();

    auto groups = groupSelectionByNote();
    auto& notes = noteSequence.getAllNotes();

    for (auto& [noteIdx, points] : groups)
    {
        if (noteIdx < 0 || noteIdx >= (int)notes.size()) continue;
        auto& curve = notes[(size_t)noteIdx].amplitudeCurve;
        if ((int)curve.size() < 2) continue;

        std::vector<int> sortedIdx = points;
        std::sort(sortedIdx.begin(), sortedIdx.end());

        int runStart = sortedIdx.empty() ? -1 : sortedIdx.front();
        int prev = runStart;
        auto applyRun = [&](int s, int e) { smoothRun(curve, s, e, intensity); };

        for (size_t k = 1; k < sortedIdx.size(); ++k)
        {
            if (sortedIdx[k] == prev + 1) { prev = sortedIdx[k]; continue; }
            applyRun(runStart, prev);
            runStart = sortedIdx[k]; prev = runStart;
        }
        if (runStart >= 0) applyRun(runStart, prev);
    }
    notifyChanged();
}

void VelocityLane::smoothPreviewBegin()
{
    if (laneMode != LaneMode::Dynamics) return;
    if (onUndoNeeded) onUndoNeeded();

    smoothPreviewActive = true;
    smoothSnapshots.clear();

    auto groups = groupSelectionByNote();
    auto& notes = noteSequence.getAllNotes();
    for (auto& [noteIdx, _points] : groups)
    {
        ignoreUnused(_points);
        if (noteIdx < 0 || noteIdx >= (int)notes.size()) continue;
        smoothSnapshots.push_back({ noteIdx, notes[(size_t)noteIdx].amplitudeCurve });
    }
}

void VelocityLane::smoothPreviewUpdate(float intensity)
{
    if (!smoothPreviewActive) return;

    auto& notes = noteSequence.getAllNotes();
    
    for (auto& snap : smoothSnapshots)
        if (snap.noteIdx >= 0 && snap.noteIdx < (int)notes.size())
            notes[(size_t)snap.noteIdx].amplitudeCurve = snap.orig;

    smoothDynamicsSelection(intensity);
}

void VelocityLane::smoothPreviewCommit()
{
    smoothPreviewActive = false;
    smoothSnapshots.clear();
}

void VelocityLane::smoothPreviewCancel()
{
    if (!smoothPreviewActive) return;
    auto& notes = noteSequence.getAllNotes();
    for (auto& snap : smoothSnapshots)
        if (snap.noteIdx >= 0 && snap.noteIdx < (int)notes.size())
            notes[(size_t)snap.noteIdx].amplitudeCurve = snap.orig;
    smoothPreviewActive = false;
    smoothSnapshots.clear();
    notifyChanged();
}
