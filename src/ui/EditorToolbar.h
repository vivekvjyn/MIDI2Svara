#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"

class EditorToolbar : public Component
{
public:
    enum class Tool { Edit, Pencil, Vibrato };
    enum class EditMode { Pitch, Midi };
    enum class SnapMode { Free, Grid };

    EditorToolbar();
    void resized() override;
    void paint(Graphics& g) override;

    Tool getCurrentTool() const { return currentTool; }
    EditMode getEditMode() const { return editMode; }
    SnapMode getSnapMode() const { return snapMode; }
    double getGridDivisionBeats() const { return gridDivisionBeats; }

    void setMidiMode(int mode)
    {
        if (mode == 0) mpeBtn.setToggleState(true, dontSendNotification);
        else monoBtn.setToggleState(true, dontSendNotification);
    }

    std::function<void(Tool)> onToolChanged;
    std::function<void(EditMode)> onEditModeChanged;
    std::function<void(SnapMode)> onSnapChanged;
    std::function<void(double)> onGridDivisionChanged;
    std::function<void()> onQuantizeClicked;
    std::function<void(bool)> onFollowToggled;
    std::function<void(int)> onMidiModeChanged;

private:
    Tool currentTool = Tool::Edit;
    EditMode editMode = EditMode::Pitch;
    SnapMode snapMode = SnapMode::Free;

    TextButton editModeBtn{"Pitch"};
    TextButton gridBtn{"Snap"};
    ComboBox snapDivisionBox;
    TextButton quantizeBtn{"Quantize"};
    double gridDivisionBeats = 0.5;

    TextButton followBtn{"Follow"};

    TextButton mpeBtn{"MPE"}, monoBtn{"Mono"};
};
