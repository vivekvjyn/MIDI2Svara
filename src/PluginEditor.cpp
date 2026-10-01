#include "PluginEditor.h"


Editor::Editor(Processor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      pianoRoll(p.getNoteSequence()),
      velocityLane(p.getNoteSequence())
{
    setLookAndFeel(&lookAndFeel);
    setSize(1100, 650);
    setResizable(true, true);
    setResizeLimits(800, 500, 2000, 1200);

    addAndMakeVisible(settingsPanel);
    pianoRoll.setSettingsPanelRef(&settingsPanel);

    toolbar.onToolChanged = [this](EditorToolbar::Tool t) { pianoRoll.setCurrentTool(t); };
    toolbar.onSnapChanged = [this](EditorToolbar::SnapMode m) { pianoRoll.setSnapMode(m); };
    toolbar.onGridDivisionChanged = [this](double beats) { pianoRoll.setGridDivision(beats); };
    toolbar.onQuantizeClicked = [this] { pianoRoll.quantizeNotes(); };
    toolbar.onEditModeChanged = [this](EditorToolbar::EditMode m) { pianoRoll.setPitchMode(m == EditorToolbar::EditMode::Pitch ? PianoRoll::PitchMode::Continuous : PianoRoll::PitchMode::Segmented); };
    pianoRoll.setGridDivision(toolbar.getGridDivisionBeats());

    toolbar.onFollowToggled = [this](bool on) { pianoRoll.setFollowEnabled(on); };

    toolbar.onMidiModeChanged = [this](int mode)
    {
        if (mode == 0) audioProcessor.setMidiOutputMode(MidiEngine::Mode::MPE);
        else audioProcessor.setMidiOutputMode(MidiEngine::Mode::Mono);
    };

    {
        auto ragas = CarnaticEngine::getAvailableRagas();
        settingsPanel.setRagas(ragas);
        settingsPanel.setRagaMode(true);
    }

    auto& rp = settingsPanel;

    rp.onRagaDone = [this](String ragaName)
    {
        bool stablePaSa = settingsPanel.isStablePaSaEnabled();
        float pitchCorrection = settingsPanel.getPitchCorrection();
        int rootNote = settingsPanel.getRootNote();
        carnaticEngine.setRootNote(60 + rootNote);
        if (carnaticEngine.loadRaga(ragaName))
        {
            auto& intervals = CarnaticEngine::getRagaIntervals();
            auto it = intervals.find(ragaName);
            if (it != intervals.end())
            {
                keyboard.setRootNote(60 + rootNote);
                keyboard.setRagaIntervals(it->second);
            }

            carnaticEngine.applyExpression(audioProcessor.getNoteSequence(), stablePaSa, pitchCorrection);
            repaint();
        }
        settingsPanel.resetToRagaMode();
    };

    rp.onRagaCancel = [this]
    {
        keyboard.clearRagaIntervals();
        settingsPanel.resetToRagaMode();
    };

    rp.onPreviewClicked = [this](String) {};
    rp.onAlgorithmChanged = [this](String) {};
    rp.onIntensityChanged = [this](float) {};
    rp.onCommitAsNotes = [this] { settingsPanel.resetToSelectionMode(); };
    rp.onCommitAsContinuous = [this] { settingsPanel.resetToSelectionMode(); };
    rp.onCommitRawAsContinuous = [this] { settingsPanel.resetToSelectionMode(); };
    rp.onCancel = [this] { settingsPanel.resetToSelectionMode(); };

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
    pianoRoll.onNotesChanged = [this] { pianoRoll.repaint(); velocityLane.repaint(); };
    pianoRoll.onViewChanged = [this] { syncViewRanges(); };
    addAndMakeVisible(pianoRoll);

    pianoRoll.onActivated = [this] {};

    velocityLane.onVelocityChanged = [this] { pianoRoll.repaint(); };
    velocityLane.onUndoNeeded = [this] { pianoRoll.saveUndoState(); };
    velocityLane.onResized = [this](int) { resized(); };
    velocityLane.onMouseWheel = [this](const MouseEvent& e, const MouseWheelDetails& wheel)
    {
        pianoRoll.mouseWheelMove(e, wheel);
    };
    velocityLane.onAutoClosed = [this] {};
    velocityLane.onLaneFocused = [this] {};
    pianoRoll.onActivated = [this] {};
    addAndMakeVisible(velocityLane);

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
    { pianoRoll.setCurrentTool(EditorToolbar::Tool::Edit); return true; }
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P')
    { pianoRoll.setCurrentTool(EditorToolbar::Tool::Pencil); return true; }
    if (key.getTextCharacter() == 'v' || key.getTextCharacter() == 'V')
    { pianoRoll.setCurrentTool(EditorToolbar::Tool::Vibrato); return true; }
    return false;
}

void Editor::syncViewRanges()
{
    double startBeat = pianoRoll.getViewStartBeat();
    double endBeat = pianoRoll.getViewEndBeat();
    double lowest = pianoRoll.getViewLowest();
    double highest = pianoRoll.getViewHighest();
    timeRuler.setViewRange(startBeat, endBeat);
    keyboard.setViewRange(lowest, highest);
    velocityLane.setViewRange(startBeat, endBeat);
}

void Editor::paint(Graphics& g) { g.fillAll(FPColours::background); }

void Editor::resized()
{
    auto area = getLocalBounds();

    auto settingsArea = area.removeFromRight(expressionPanelWidth);
    settingsPanel.setBounds(settingsArea);

    toolbar.setBounds(area.removeFromTop(toolbarHeight));

    auto rulerArea = area.removeFromTop(25);
    rulerArea.removeFromLeft(60);
    timeRuler.setBounds(rulerArea);

    auto velArea = area.removeFromBottom(velocityLaneHeight);
    velArea.removeFromLeft(60);
    velocityLane.setBounds(velArea);

    auto keyboardArea = area.removeFromLeft(60);
    keyboard.setBounds(keyboardArea);

    pianoRoll.setBounds(area);

    pianoRoll.setVisibleWidthFraction(1.0f);
}
