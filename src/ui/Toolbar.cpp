#include "Toolbar.h"

::Toolbar::Toolbar()
{
    auto handCursor = MouseCursor::PointingHandCursor;

    moveBtn.setClickingTogglesState(true);
    moveBtn.setRadioGroupId(1001);
    moveBtn.setTooltip("Move notes");
    moveBtn.onClick = [this] { setTool(Tool::Move); };
    addAndMakeVisible(moveBtn);

    pencilBtn.setClickingTogglesState(true);
    pencilBtn.setRadioGroupId(1001);
    pencilBtn.setToggleState(true, dontSendNotification);
    pencilBtn.setTooltip("Pencil — draw notes");
    pencilBtn.onClick = [this] { setTool(Tool::Pencil); };
    addAndMakeVisible(pencilBtn);

    ragaCombo.setTooltip("Raga");
    addAndMakeVisible(ragaCombo);

    tonicLabel.setText("Sa", dontSendNotification);
    tonicLabel.setFont(FontOptions(12.0f));
    tonicLabel.setColour(Label::textColourId, FPColours::textDim);
    tonicLabel.setJustificationType(Justification::centred);
    addAndMakeVisible(tonicLabel);

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; i++)
        tonicCombo.addItem(noteNames[i], i + 1);
    tonicCombo.setSelectedId(1, dontSendNotification);
    addAndMakeVisible(tonicCombo);

    refreshBtn.setTooltip("Apply raga");
    refreshBtn.onClick = [this] { if (onRefreshClicked) onRefreshClicked(); };
    addAndMakeVisible(refreshBtn);

    mpeSwitch.setClickingTogglesState(true);
    mpeSwitch.setToggleState(true, dontSendNotification);
    mpeSwitch.setTooltip("MPE output");
    mpeSwitch.onClick = [this] { if (onMidiModeChanged) onMidiModeChanged(mpeSwitch.getToggleState() ? 0 : 1); };
    addAndMakeVisible(mpeSwitch);

    Array<Component*> interactive {
        &moveBtn, &pencilBtn, &ragaCombo, &tonicCombo, &refreshBtn, &mpeSwitch
    };
    for (auto* c : interactive)
        c->setMouseCursor(handCursor);
}

void ::Toolbar::setRagas(const StringArray& list)
{
    ragas = list;
    ragaCombo.clear(dontSendNotification);

    int id = 1;
    for (auto& r : ragas)
        ragaCombo.addItem(r.substring(0, 1).toUpperCase() + r.substring(1), id++);

    if (ragas.size() > 0)
        ragaCombo.setSelectedId(1, dontSendNotification);
}

auto ::Toolbar::getSelectedRaga() const -> String
{
    int i = ragaCombo.getSelectedId() - 1;
    if (i >= 0 && i < ragas.size())
        return ragas[i];
    return {};
}

void ::Toolbar::paint(Graphics& g)
{
    g.fillAll(FPColours::toolbarBg);
    g.setColour(FPColours::gridLine);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
}

void ::Toolbar::resized()
{
    const int gap = 8;
    auto area = getLocalBounds().reduced(8, 7);

    auto right = area;
    mpeSwitch.setBounds(right.removeFromRight(70));
    right.removeFromRight(gap);
    refreshBtn.setBounds(right.removeFromRight(30));
    right.removeFromRight(gap);
    tonicCombo.setBounds(right.removeFromRight(58));
    right.removeFromRight(4);
    tonicLabel.setBounds(right.removeFromRight(24));
    right.removeFromRight(gap);
    ragaCombo.setBounds(right.removeFromRight(jmin(160, jmax(120, right.getWidth() / 3))));
    right.removeFromRight(gap * 2);

    auto left = area;
    pencilBtn.setBounds(left.removeFromLeft(30));
    left.removeFromLeft(gap);
    moveBtn.setBounds(left.removeFromLeft(30));
}
