#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"


class SettingsPanel : public Component
{
public:
    SettingsPanel();
    void resized() override;
    void paint(Graphics& g) override;

    void setAlgorithms(const std::vector<std::tuple<String, String, bool>>& algos);

    void setRagas(const StringArray& ragas, int defaultIndex = 0);
    String getSelectedRaga() const;

    bool isStablePaSaEnabled() const { return stablePaSaToggle.getToggleState(); }
    float getPitchCorrection() const { return (float)pitchCorrectionSlider.getValue(); }
    int getRootNote() const { return rootNoteCombo.getSelectedId() - 1; }

    void setRagaMode(bool active);
    bool isInRagaMode() const { return ragaMode; }

    String getSelectedAlgorithmId() const;

    void setPreviewMode(bool active);
    bool isInPreviewMode() const { return previewMode; }

    void resetToRagaMode();
    void resetToSelectionMode();

    std::function<void(String)> onRagaDone;
    std::function<void()> onRagaCancel;
    std::function<void(String)> onPreviewClicked;
    std::function<void(String)> onAlgorithmChanged;
    std::function<void(float)> onIntensityChanged;
    std::function<void(bool)> onShowRawMLChanged;
    std::function<void(bool)> onShowBlendedChanged;
    std::function<void()> onCommitAsNotes;
    std::function<void()> onCommitAsContinuous;
    std::function<void()> onCommitRawAsContinuous;
    std::function<void()> onCancel;
    std::function<void(int)> onRootNoteChanged;

private:
    bool ragaMode = true;
    bool previewMode = false;

    Label titleLabel;
    Label ragaLabel;
    ComboBox ragaCombo;
    Label rootNoteLabel;
    ComboBox rootNoteCombo;
    ToggleButton stablePaSaToggle { "Stable Pa & Sa" };
    Label pitchCorrectionLabel;
    Slider pitchCorrectionSlider;
    TextButton ragaDoneBtn { "Done" };
    TextButton ragaCancelBtn { "Cancel" };

    Label algoLabel;
    ComboBox algoCombo;
    std::vector<String> algoIds;

    TextButton previewBtn { "Preview" };

    Label intensityLabel;
    Slider intensitySlider;
    ToggleButton showRawMLToggle { "Show Raw ML" };
    ToggleButton showBlendedToggle { "Show Blended" };
    TextButton commitNotesBtn { "Accept as Notes" };
    TextButton commitContinuousBtn { "Accept as Continuous" };
    TextButton commitRawBtn { "Accept Raw ML" };
    TextButton cancelBtn { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
