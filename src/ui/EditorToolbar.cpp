#include "EditorToolbar.h"


EditorToolbar::EditorToolbar()
{
    auto handCursor = MouseCursor::PointingHandCursor;

    editModeBtn.setClickingTogglesState(true);
    editModeBtn.onClick = [this]
    {
        editMode = editModeBtn.getToggleState() ? EditMode::Midi : EditMode::Pitch;
        editModeBtn.setButtonText(editMode == EditMode::Pitch ? "Pitch" : "Midi");
        if (onEditModeChanged) onEditModeChanged(editMode);
    };
    addAndMakeVisible(editModeBtn);

    gridBtn.setClickingTogglesState(true);
    gridBtn.onClick = [this]
    {
        snapMode = gridBtn.getToggleState() ? SnapMode::Grid : SnapMode::Free;
        if (onSnapChanged) onSnapChanged(snapMode);
    };
    addAndMakeVisible(gridBtn);

    struct DivEntry { const char* label; double beats; };
    static const DivEntry kDivisions[] = {
        { "1 bar", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 },
        { "1/8", 0.5 },   { "1/16", 0.25 }, { "1/32", 0.125 }
    };
    for (int i = 0; i < 6; ++i)
        snapDivisionBox.addItem(kDivisions[i].label, i + 1);
    snapDivisionBox.setSelectedId(4, dontSendNotification);
    gridDivisionBeats = kDivisions[3].beats;
    snapDivisionBox.onChange = [this]
    {
        static const double beatsById[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        int id = snapDivisionBox.getSelectedId();
        if (id >= 1 && id <= 6)
        {
            gridDivisionBeats = beatsById[id];
            if (onGridDivisionChanged) onGridDivisionChanged(gridDivisionBeats);
        }
    };
    addAndMakeVisible(snapDivisionBox);

    quantizeBtn.onClick = [this] { if (onQuantizeClicked) onQuantizeClicked(); };
    addAndMakeVisible(quantizeBtn);

    followBtn.setClickingTogglesState(true);
    followBtn.setToggleState(true, dontSendNotification);
    followBtn.onClick = [this] { if (onFollowToggled) onFollowToggled(followBtn.getToggleState()); };
    addAndMakeVisible(followBtn);

    mpeBtn.setClickingTogglesState(true);
    mpeBtn.setRadioGroupId(1);
    mpeBtn.setToggleState(true, dontSendNotification);
    mpeBtn.onClick = [this] { if (onMidiModeChanged) onMidiModeChanged(0); };
    addAndMakeVisible(mpeBtn);

    monoBtn.setClickingTogglesState(true);
    monoBtn.setRadioGroupId(1);
    monoBtn.onClick = [this] { if (onMidiModeChanged) onMidiModeChanged(1); };
    addAndMakeVisible(monoBtn);

    Array<Component*> interactive {
        &editModeBtn, &gridBtn, &snapDivisionBox, &quantizeBtn,
        &followBtn, &mpeBtn, &monoBtn
    };
    for (auto* c : interactive)
        c->setMouseCursor(handCursor);
}

void EditorToolbar::paint(Graphics& g)
{
    g.fillAll(FPColours::toolbarBg);
    g.setColour(FPColours::gridLine);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
}

void EditorToolbar::resized()
{
    const int gap = 8;
    auto area = getLocalBounds().reduced(6, 4);

    auto right = area;
    monoBtn.setBounds(right.removeFromRight(48).reduced(0, 2));
    right.removeFromRight(gap);
    mpeBtn.setBounds(right.removeFromRight(44).reduced(0, 2));
    right.removeFromRight(gap * 2);

    auto left = area;
    editModeBtn.setBounds(left.removeFromLeft(48).reduced(0, 2));
    left.removeFromLeft(gap);
    gridBtn.setBounds(left.removeFromLeft(48).reduced(0, 2));
    left.removeFromLeft(gap);
    snapDivisionBox.setBounds(left.removeFromLeft(72).reduced(0, 2));
    left.removeFromLeft(gap);
    quantizeBtn.setBounds(left.removeFromLeft(68).reduced(0, 2));
    left.removeFromLeft(gap * 2);

    followBtn.setBounds(left.removeFromLeft(56).reduced(0, 2));
}
