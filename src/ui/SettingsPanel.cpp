#include "SettingsPanel.h"


SettingsPanel::SettingsPanel()
{
    
    titleLabel.setText("Settings", dontSendNotification);
    titleLabel.setFont(FontOptions(15.0f).withStyle("Bold"));
    titleLabel.setColour(Label::textColourId, FPColours::text);
    titleLabel.setJustificationType(Justification::centred);
    addAndMakeVisible(titleLabel);

    ragaLabel.setText("Raga", dontSendNotification);
    ragaLabel.setFont(FontOptions(11.0f));
    ragaLabel.setColour(Label::textColourId, FPColours::textDim);
    addAndMakeVisible(ragaLabel);

    addAndMakeVisible(ragaCombo);

    rootNoteLabel.setText("Root Note", dontSendNotification);
    rootNoteLabel.setFont(FontOptions(11.0f));
    rootNoteLabel.setColour(Label::textColourId, FPColours::textDim);
    addAndMakeVisible(rootNoteLabel);

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; i++)
        rootNoteCombo.addItem(noteNames[i], i + 1);
    rootNoteCombo.setSelectedId(1, dontSendNotification);
    rootNoteCombo.onChange = [this]
    {
        if (onRootNoteChanged)
            onRootNoteChanged(getRootNote());
    };
    addAndMakeVisible(rootNoteCombo);

    stablePaSaToggle.setColour(ToggleButton::textColourId, FPColours::textDim);
    stablePaSaToggle.setToggleState(true, dontSendNotification);
    addAndMakeVisible(stablePaSaToggle);

    pitchCorrectionLabel.setText("Pitch Correction", dontSendNotification);
    pitchCorrectionLabel.setFont(FontOptions(11.0f));
    pitchCorrectionLabel.setColour(Label::textColourId, FPColours::textDim);
    addAndMakeVisible(pitchCorrectionLabel);

    pitchCorrectionSlider.setRange(0.0, 1.0, 0.01);
    pitchCorrectionSlider.setValue(0.0, dontSendNotification);
    pitchCorrectionSlider.setSliderStyle(Slider::LinearHorizontal);
    pitchCorrectionSlider.setTextBoxStyle(Slider::TextBoxRight, false, 36, 18);
    pitchCorrectionSlider.setNumDecimalPlacesToDisplay(2);
    pitchCorrectionSlider.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(pitchCorrectionSlider);

    ragaDoneBtn.setColour(TextButton::buttonColourId, FPColours::vibrato);
    ragaDoneBtn.onClick = [this]
    {
        auto raga = getSelectedRaga();
        if (raga.isNotEmpty() && onRagaDone)
            onRagaDone(raga);
    };
    addAndMakeVisible(ragaDoneBtn);

    ragaCancelBtn.setColour(TextButton::buttonColourId, FPColours::gamaka);
    ragaCancelBtn.onClick = [this] { if (onRagaCancel) onRagaCancel(); };
    addAndMakeVisible(ragaCancelBtn);

    algoLabel.setText("Algorithm", dontSendNotification);
    algoLabel.setFont(FontOptions(11.0f));
    algoLabel.setColour(Label::textColourId, FPColours::textDim);
    addAndMakeVisible(algoLabel);

    algoCombo.onChange = [this]
    {
        if (!previewMode) return; 
        int selId = algoCombo.getSelectedId();
        if (selId <= 0) return;
        int idx = selId - 1;
        if (idx < 0 || idx >= (int)algoIds.size()) return;
        if (onAlgorithmChanged) onAlgorithmChanged(algoIds[(size_t)idx]);
    };
    addAndMakeVisible(algoCombo);

    previewBtn.setColour(TextButton::buttonColourId, FPColours::accentCyan);
    previewBtn.onClick = [this]
    {
        auto algoId = getSelectedAlgorithmId();
        if (algoId.isNotEmpty() && onPreviewClicked)
            onPreviewClicked(algoId);
    };
    addAndMakeVisible(previewBtn);

    intensityLabel.setText("Intensity", dontSendNotification);
    intensityLabel.setFont(FontOptions(11.0f));
    intensityLabel.setColour(Label::textColourId, FPColours::textDim);
    addChildComponent(intensityLabel);

    intensitySlider.setRange(0.0, 1.0, 0.01);
    intensitySlider.setValue(0.7, dontSendNotification);
    intensitySlider.setSliderStyle(Slider::LinearHorizontal);
    intensitySlider.setTextBoxStyle(Slider::TextBoxRight, false, 36, 18);
    intensitySlider.setNumDecimalPlacesToDisplay(2);
    intensitySlider.setDoubleClickReturnValue(true, 0.7);
    intensitySlider.onValueChange = [this]
    {
        if (onIntensityChanged) onIntensityChanged((float)intensitySlider.getValue());
    };
    addChildComponent(intensitySlider);

    showRawMLToggle.setToggleState(true, dontSendNotification);
    showRawMLToggle.onClick = [this]
    {
        if (onShowRawMLChanged) onShowRawMLChanged(showRawMLToggle.getToggleState());
    };
    addChildComponent(showRawMLToggle);

    showBlendedToggle.setToggleState(true, dontSendNotification);
    showBlendedToggle.onClick = [this]
    {
        if (onShowBlendedChanged) onShowBlendedChanged(showBlendedToggle.getToggleState());
    };
    addChildComponent(showBlendedToggle);

    commitNotesBtn.setColour(TextButton::buttonColourId, FPColours::vibrato);
    commitNotesBtn.onClick = [this] { if (onCommitAsNotes) onCommitAsNotes(); };
    addChildComponent(commitNotesBtn);

    commitContinuousBtn.setColour(TextButton::buttonColourId, FPColours::accentCyan);
    commitContinuousBtn.onClick = [this] { if (onCommitAsContinuous) onCommitAsContinuous(); };
    addChildComponent(commitContinuousBtn);

    commitRawBtn.setColour(TextButton::buttonColourId, FPColours::gamaka);
    commitRawBtn.onClick = [this] { if (onCommitRawAsContinuous) onCommitRawAsContinuous(); };
    addChildComponent(commitRawBtn);

    cancelBtn.setColour(TextButton::buttonColourId, FPColours::gamaka);
    cancelBtn.onClick = [this] { if (onCancel) onCancel(); };
    addChildComponent(cancelBtn);

    auto handCursor = MouseCursor::PointingHandCursor;
    for (auto* c : { (Component*)&ragaCombo, (Component*)&rootNoteCombo, (Component*)&stablePaSaToggle,
                     (Component*)&pitchCorrectionSlider,
                     (Component*)&ragaDoneBtn, (Component*)&ragaCancelBtn,
                     (Component*)&algoCombo, (Component*)&intensitySlider,
                     (Component*)&previewBtn,
                     (Component*)&showRawMLToggle, (Component*)&showBlendedToggle,
                     (Component*)&commitNotesBtn, (Component*)&commitContinuousBtn,
                     (Component*)&commitRawBtn, (Component*)&cancelBtn })
        c->setMouseCursor(handCursor);
}

void SettingsPanel::setAlgorithms(
    const std::vector<std::tuple<String, String, bool>>& algos)
{
    algoCombo.clear(dontSendNotification);
    algoIds.clear();
    int itemId = 1;
    int firstEnabled = 0;
    for (auto& [id, display, enabled] : algos)
    {
        algoCombo.addItem(display, itemId);
        algoCombo.setItemEnabled(itemId, enabled);
        algoIds.push_back(id);
        if (enabled && firstEnabled == 0)
            firstEnabled = itemId;
        ++itemId;
    }
    if (firstEnabled > 0)
        algoCombo.setSelectedId(firstEnabled, dontSendNotification);
}

void SettingsPanel::setRagas(const StringArray& ragas, int defaultIndex)
{
    ragaCombo.clear(dontSendNotification);
    int itemId = 1;
    for (auto& raga : ragas)
    {
        
        auto displayName = raga.substring(0, 1).toUpperCase() + raga.substring(1);
        ragaCombo.addItem(displayName, itemId++);
    }
    if (ragas.size() > 0)
    {
        int selIdx = jlimit(0, ragas.size() - 1, defaultIndex);
        ragaCombo.setSelectedId(selIdx + 1, dontSendNotification);
    }
}

String SettingsPanel::getSelectedRaga() const
{
    int selId = ragaCombo.getSelectedId();
    if (selId <= 0) return {};
    int idx = selId - 1;
    auto ragas = StringArray::fromTokens("abhogi,begada,kalyani,mohanam,sahana,saveri,sri", ",", "");
    if (idx >= 0 && idx < ragas.size())
        return ragas[idx];
    return {};
}

void SettingsPanel::setRagaMode(bool active)
{
    ragaMode = active;

    ragaLabel.setVisible(active);
    ragaCombo.setVisible(active);
    rootNoteLabel.setVisible(active);
    rootNoteCombo.setVisible(active);
    ragaDoneBtn.setVisible(active);
    ragaCancelBtn.setVisible(active);

    if (!previewMode)
    {
        algoLabel.setVisible(!active);
        algoCombo.setVisible(!active);
        previewBtn.setVisible(!active);
    }

    resized();
}

void SettingsPanel::resetToRagaMode()
{
    previewMode = false;
    ragaMode = true;
    intensitySlider.setValue(0.7, dontSendNotification);
    showRawMLToggle.setToggleState(true, dontSendNotification);
    showBlendedToggle.setToggleState(true, dontSendNotification);
    pitchCorrectionSlider.setValue(0.0, dontSendNotification);
    setRagaMode(true);
}

String SettingsPanel::getSelectedAlgorithmId() const
{
    int selId = algoCombo.getSelectedId();
    if (selId <= 0) return {};
    int idx = selId - 1;
    if (idx < 0 || idx >= (int)algoIds.size()) return {};
    return algoIds[(size_t)idx];
}

void SettingsPanel::setPreviewMode(bool active)
{
    previewMode = active;

    if (active)
    {
        
        ragaMode = false;
        ragaLabel.setVisible(false);
        ragaCombo.setVisible(false);
        ragaDoneBtn.setVisible(false);
        ragaCancelBtn.setVisible(false);
        algoLabel.setVisible(false);
        algoCombo.setVisible(false);
        previewBtn.setVisible(false);
    }
    else
    {
        
        ragaLabel.setVisible(false);
        ragaCombo.setVisible(false);
        ragaDoneBtn.setVisible(false);
        ragaCancelBtn.setVisible(false);
        algoLabel.setVisible(true);
        algoCombo.setVisible(true);
        previewBtn.setVisible(true);
    }

    intensityLabel.setVisible(active);
    intensitySlider.setVisible(active);
    showRawMLToggle.setVisible(active);
    showBlendedToggle.setVisible(active);
    commitNotesBtn.setVisible(active);
    commitContinuousBtn.setVisible(active);
    commitRawBtn.setVisible(active);
    cancelBtn.setVisible(active);

    resized();
}

void SettingsPanel::resetToSelectionMode()
{
    intensitySlider.setValue(0.7, dontSendNotification);
    showRawMLToggle.setToggleState(true, dontSendNotification);
    showBlendedToggle.setToggleState(true, dontSendNotification);
    setPreviewMode(false);
}

void SettingsPanel::paint(Graphics& g)
{
    g.fillAll(FPColours::settingsBg);

    g.setColour(FPColours::settingsBorder.withAlpha(0.6f));
    g.drawVerticalLine(0, 0.0f, (float)getHeight());

    g.setColour(FPColours::settingsBorder);
    g.fillRect(0, 0, getWidth(), 2);

    g.setColour(FPColours::gridLine.withAlpha(0.5f));
    g.drawHorizontalLine(56, 10.0f, (float)(getWidth() - 10));
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    const int rowH = 28;
    const int gap = 8;

    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(gap);

    if (ragaMode)
    {
        const int contentH = 16 + 4 + rowH + 8 + 16 + 4 + rowH + 12 + 22 + 10 + 14 + 4 + rowH + 20 + 36 + gap + 36;
        int topPad = (area.getHeight() - contentH) / 2;
        if (topPad < 0) topPad = 0;
        area.removeFromTop(topPad);

        ragaLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(4);
        ragaCombo.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(8);
        rootNoteLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(4);
        rootNoteCombo.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(12);
        stablePaSaToggle.setBounds(area.removeFromTop(22));
        area.removeFromTop(10);
        pitchCorrectionLabel.setBounds(area.removeFromTop(14));
        area.removeFromTop(4);
        pitchCorrectionSlider.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(20);

        const int btnH = 36;
        ragaDoneBtn.setBounds(area.removeFromTop(btnH));
        area.removeFromTop(gap);
        ragaCancelBtn.setBounds(area.removeFromTop(btnH));
    }
    else if (!previewMode)
    {
        
        algoLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(4);
        algoCombo.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(gap + 4);

        const int btnH = 36;
        previewBtn.setBounds(area.removeFromTop(btnH));
    }
    else
    {
        
        intensityLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(4);
        intensitySlider.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(gap + 4);

        showRawMLToggle.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(4);
        showBlendedToggle.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(gap + 8);

        const int btnH = 32;
        commitNotesBtn.setBounds(area.removeFromTop(btnH));
        area.removeFromTop(gap);
        commitContinuousBtn.setBounds(area.removeFromTop(btnH));
        area.removeFromTop(gap);
        commitRawBtn.setBounds(area.removeFromTop(btnH));
        area.removeFromTop(gap + 4);

        cancelBtn.setBounds(area.removeFromTop(btnH));
    }
}
