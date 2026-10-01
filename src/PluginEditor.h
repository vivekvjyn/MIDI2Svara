#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/PianoRoll.h"
#include "ui/Keyboard.h"
#include "ui/TimeRuler.h"
#include "ui/EditorToolbar.h"
#include "ui/VelocityLane.h"
#include "ui/SettingsPanel.h"
#include "dsp/CarnaticEngine.h"

class Editor : public AudioProcessorEditor,
                          public Timer,
                          public DragAndDropContainer
{
public:
    explicit Editor(Processor&);
    ~Editor() override;

    void paint(Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const KeyPress& key) override;

private:
    Processor& audioProcessor;
    AppLookAndFeel lookAndFeel;

    EditorToolbar toolbar;
    TimeRuler timeRuler;
    Keyboard keyboard;
    PianoRoll pianoRoll;
    VelocityLane velocityLane;


    CarnaticEngine carnaticEngine;
    SettingsPanel settingsPanel;

    double playbackStartBeat = 0.0;

    static constexpr int toolbarHeight = 36;
    static constexpr int velocityLaneHeight = 120;
    static constexpr int expressionPanelWidth = 180;

    void syncViewRanges();
    void triggerNotePlayback(NoteData* note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
