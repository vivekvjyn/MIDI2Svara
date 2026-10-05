#include "Editor.h"

Editor::Editor(Processor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      pianoRoll(p.getNoteSequence())
{
    setLookAndFeel(&lookAndFeel);
    setSize(1100, 650);
    setResizable(true, true);
    setResizeLimits(800, 500, 2000, 1200);

    setWantsKeyboardFocus(true);
    tooltipWindow = std::make_unique<TooltipWindow>(this, 700);

    toolbar.onToolChanged = [this](::Toolbar::Tool t)
    {
        pianoRoll.setCurrentTool(t);
    };

    toolbar.onMidiModeChanged = [this](int mode)
    {
        if (mode == 0) audioProcessor.setMidiOutputMode(MidiOut::Mode::MPE);
        else audioProcessor.setMidiOutputMode(MidiOut::Mode::Mono);
    };

    toolbar.setMidiMode(audioProcessor.getMidiOutputMode() == MidiOut::Mode::MPE ? 0 : 1);

    toolbar.setRagas(Engine::getAvailableRagas());
    toolbar.onRefreshClicked = [this] { applyRagaSelection(true); };

    applyRagaSelection(false);

    addAndMakeVisible(toolbar);

    audioProcessor.onStateLoaded = [this]
    {
        pianoRoll.repaint();
        syncViewRanges();
        repaint();
    };

    timeRuler.onPositionClicked = [this](double beat)
    {
        playbackStartBeat = beat;
        audioProcessor.setPlayheadBeat(beat);
        pianoRoll.setPlayheadPosition(beat);
    };
    timeRuler.setMouseCursor(MouseCursor::PointingHandCursor);
    addAndMakeVisible(timeRuler);

    keyboard.onKeyAction = [this](int note, float velocity, bool isDown)
    {
        if (isDown) audioProcessor.injectNoteOn(note, velocity);
        else audioProcessor.injectNoteOff(note);
    };
    keyboard.onMouseWheel = [this](const MouseEvent& e, const MouseWheelDetails& wheel)
    {
        pianoRoll.mouseWheelMove(e, wheel);
    };
    keyboard.setMouseCursor(MouseCursor::PointingHandCursor);
    addAndMakeVisible(keyboard);

    pianoRoll.onNoteSelected = [this](NoteData* note)
    {
        if (note != nullptr) triggerNotePlayback(note);
    };
    pianoRoll.onNotesChanged = [this] { pianoRoll.repaint(); };
    pianoRoll.onViewChanged = [this] { syncViewRanges(); };
    addAndMakeVisible(pianoRoll);

    audioProcessor.onBeforeRecordingMerge = [this] { pianoRoll.saveUndoState(); };
    audioProcessor.onRecordingFinished = [this] { pianoRoll.repaint(); };

    pianoRoll.setViewRange(0.0, 16.0, 55.0, 72.0);
    syncViewRanges();
    startTimerHz(30);
}

Editor::~Editor()
{
    audioProcessor.onBeforeRecordingMerge = nullptr;
    audioProcessor.onRecordingFinished = nullptr;
    setLookAndFeel(nullptr);
}

void Editor::timerCallback()
{
    double beat = audioProcessor.getPlayheadBeat();
    pianoRoll.setPlayheadPosition(beat);
    timeRuler.setPlayheadPosition(beat);

    if (pianoRoll.updateFollow(beat, audioProcessor.isPlaying()))
        syncViewRanges();

    pianoRoll.setRecording(audioProcessor.isRecording());

    if (audioProcessor.isRecording())
    {
        auto preview = audioProcessor.getRecordingPreview();
        std::vector<PianoRoll::RecPreviewNote> previewNotes;
        previewNotes.reserve(preview.size());
        for (auto& p : preview)
        {
            PianoRoll::RecPreviewNote rn;
            rn.noteNumber = p.noteNumber;
            rn.velocity = p.velocity;
            rn.startBeat = p.startBeat;
            rn.durationBeats = p.durationBeats;
            rn.pitchCurve.reserve(p.pitchCurve.size());
            for (auto& pt : p.pitchCurve)
                rn.pitchCurve.push_back({ pt.relTime, pt.offset });
            previewNotes.push_back(std::move(rn));
        }
        pianoRoll.setRecordingPreview(previewNotes);

        double punchStart, punchEnd;
        if (audioProcessor.getPunchRange(punchStart, punchEnd))
            pianoRoll.setPunchZone(true, punchStart, punchEnd);
        else
            pianoRoll.setPunchZone(false);
    }
    else
    {
        pianoRoll.setRecordingPreview({});
        pianoRoll.setPunchZone(false);
    }

    auto state = audioProcessor.getLiveNoteState();
    keyboard.setActiveNotes(state.activeNotes);
}

void Editor::triggerNotePlayback(NoteData* note)
{
    if (note == nullptr) return;
    audioProcessor.injectNoteOn(note->noteNumber, note->velocity);
    Timer::callAfterDelay(300, [this, noteNum = note->noteNumber]()
    {
        audioProcessor.injectNoteOff(noteNum);
    });
}

bool Editor::keyPressed(const KeyPress& key)
{
    if (key == KeyPress::spaceKey)
    {
        audioProcessor.setPlaying(!audioProcessor.isPlaying());
        return true;
    }
    if (key == KeyPress('a', ModifierKeys::commandModifier, 0))
    { pianoRoll.selectAll(); return true; }
    if (key == KeyPress('z', ModifierKeys::commandModifier, 0))
    { pianoRoll.undo(); return true; }
    if (key == KeyPress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0))
    { pianoRoll.redo(); return true; }
    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
        return pianoRoll.deleteSelection();
    if (key.getKeyCode() == KeyPress::upKey)
    { pianoRoll.transposeSelection(key.getModifiers().isShiftDown() ? 12 : 1); return true; }
    if (key.getKeyCode() == KeyPress::downKey)
    { pianoRoll.transposeSelection(key.getModifiers().isShiftDown() ? -12 : -1); return true; }
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E')
    { toolbar.setTool(::Toolbar::Tool::Edit); return true; }
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P')
    { toolbar.setTool(::Toolbar::Tool::Pencil); return true; }
    if (key.getTextCharacter() == 'v' || key.getTextCharacter() == 'V')
    { toolbar.setTool(::Toolbar::Tool::Vibrato); return true; }
    if (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M')
    { toolbar.setTool(::Toolbar::Tool::Move); return true; }
    return false;
}

void Editor::applyRagaSelection(bool withExpression)
{
    const String raga = toolbar.getSelectedRaga();
    if (raga.isEmpty()) return;

    const int rootNote = 60 + toolbar.getSelectedTonic();

    const auto& intervals = Engine::getRagaIntervals();
    auto it = intervals.find(raga);
    if (it == intervals.end()) return;

    engine.setRootNote(rootNote);

    keyboard.setRootNote(rootNote);
    keyboard.setRagaIntervals(it->second);
    pianoRoll.setRagaScale(it->second, rootNote, Engine::getRagaVadi(raga));

    if (withExpression && engine.loadRaga(raga))
        engine.applyExpression(audioProcessor.getNoteSequence());

    repaint();
}

void Editor::syncViewRanges()
{
    double startBeat = pianoRoll.getViewStartBeat();
    double endBeat = pianoRoll.getViewEndBeat();
    double lowest = pianoRoll.getViewLowest();
    double highest = pianoRoll.getViewHighest();
    timeRuler.setViewRange(startBeat, endBeat);
    keyboard.setViewRange(lowest, highest);
}

void Editor::paint(Graphics& g) { g.fillAll(FPColours::background); }

void Editor::resized()
{
    auto area = getLocalBounds();

    toolbar.setBounds(area.removeFromTop(toolbarHeight));

    auto rulerArea = area.removeFromTop(25);
    rulerArea.removeFromLeft(60);
    timeRuler.setBounds(rulerArea);

    auto keyboardArea = area.removeFromLeft(60);
    keyboard.setBounds(keyboardArea);

    pianoRoll.setBounds(area);

    pianoRoll.setVisibleWidthFraction(1.0f);
}
