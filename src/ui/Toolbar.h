#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"
#include "IconButton.h"
#include "ToggleSwitch.h"

class Toolbar : public Component
{
public:
    enum class Tool { Edit, Move, Pencil, Vibrato };

    Toolbar();
    void resized() override;
    void paint(Graphics& g) override;

    void setTool(Tool t)
    {
        currentTool = t;
        moveBtn.setToggleState(t == Tool::Move, dontSendNotification);
        pencilBtn.setToggleState(t == Tool::Pencil, dontSendNotification);
        if (onToolChanged) onToolChanged(t);
    }

    void setMidiMode(int mode)
    {
        mpeSwitch.setToggleState(mode == 0, dontSendNotification);
    }

    void setRagas(const StringArray& ragas);
    String getSelectedRaga() const;
    int getSelectedTonic() const { return tonicCombo.getSelectedId() - 1; }

    std::function<void(Tool)> onToolChanged;
    std::function<void(int)> onMidiModeChanged;
    std::function<void()> onRefreshClicked;

private:
    Tool currentTool = Tool::Pencil;
    StringArray ragas;

    IconButton pencilBtn { "Pencil", IconButton::Icon::Pencil };
    IconButton moveBtn   { "Move",  IconButton::Icon::Move };

    ComboBox ragaCombo;
    Label tonicLabel;
    ComboBox tonicCombo;
    IconButton refreshBtn { "Refresh", IconButton::Icon::Refresh };

    ToggleSwitch mpeSwitch;
};
