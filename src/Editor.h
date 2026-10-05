#pragma once
#include <JuceHeader.h>
#include "Processor.h"
#include "ui/LookAndFeel.h"
#include "ui/PianoRoll.h"
#include "ui/Keyboard.h"
#include "ui/TimeRuler.h"
#include "ui/Toolbar.h"
#include "dsp/Engine.h"

class Editor : public AudioProcessorEditor,
                          public Timer
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

    ::Toolbar toolbar;
    TimeRuler timeRuler;
    Keyboard keyboard;
    PianoRoll pianoRoll;

    Engine engine;

    std::unique_ptr<TooltipWindow> tooltipWindow;

    double playbackStartBeat = 0.0;

    static constexpr int toolbarHeight = 40;

    void applyRagaSelection(bool withExpression);
    void syncViewRanges();
    void triggerNotePlayback(NoteData* note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
