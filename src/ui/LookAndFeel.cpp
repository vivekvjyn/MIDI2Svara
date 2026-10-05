#include "LookAndFeel.h"

AppLookAndFeel::AppLookAndFeel()
{
    setColour(ResizableWindow::backgroundColourId, FPColours::background);
    setColour(TextButton::buttonColourId, FPColours::surface);
    setColour(TextButton::textColourOffId, FPColours::text);
    setColour(TextButton::textColourOnId, FPColours::surface);
    setColour(ComboBox::backgroundColourId, FPColours::surface);
    setColour(ComboBox::textColourId, FPColours::text);
    setColour(ComboBox::outlineColourId, FPColours::gridLine);
    setColour(ComboBox::arrowColourId, FPColours::textDim);
    setColour(Slider::backgroundColourId, FPColours::surface);
    setColour(Slider::trackColourId, FPColours::accent);
    setColour(Slider::thumbColourId, FPColours::text);
    setColour(Label::textColourId, FPColours::text);
    setColour(PopupMenu::backgroundColourId, FPColours::surface);
    setColour(PopupMenu::textColourId, FPColours::text);
    setColour(PopupMenu::highlightedBackgroundColourId, FPColours::buttonActive);
    setColour(PopupMenu::highlightedTextColourId, FPColours::text);
    setColour(TooltipWindow::backgroundColourId, FPColours::surface);
    setColour(TooltipWindow::textColourId, FPColours::text);
    setColour(ToggleButton::textColourId, FPColours::text);
    setColour(ToggleButton::tickColourId, FPColours::text);
    setColour(TextEditor::backgroundColourId, FPColours::surface);
    setColour(TextEditor::textColourId, FPColours::text);
    setColour(TextEditor::outlineColourId, FPColours::gridLine);
}

void AppLookAndFeel::drawButtonBackground(Graphics& g, Button& button,
                                                   const Colour&,
                                                   bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    float corner = 6.0f;

    Colour base = FPColours::surface;

    if (button.getToggleState() || isButtonDown)
        base = FPColours::text;
    else if (isMouseOver)
        base = FPColours::buttonHover;

    g.setColour(base);
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(base == FPColours::text ? FPColours::text : FPColours::gridLine);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);
}

void AppLookAndFeel::drawButtonText(Graphics& g, TextButton& button,
                                            bool , bool )
{
    g.setColour(button.getToggleState()
                    ? FPColours::surface
                    : button.findColour(TextButton::textColourOffId));
    g.setFont(FontOptions(13.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds(), Justification::centred);
}

void AppLookAndFeel::drawComboBox(Graphics& g, int width, int height, bool isButtonDown,
                                          int, int, int, int, ComboBox& box)
{
    auto bounds = Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
    float corner = 6.0f;

    Colour base = box.isEnabled() ? FPColours::surface : FPColours::surfaceLight;
    if (isButtonDown || box.isPopupActive())
        base = FPColours::buttonHover;

    g.setColour(base);
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(FPColours::gridLine);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    float arrowX = (float)width - 18.0f;
    float arrowY = bounds.getCentreY();

    g.setColour(FPColours::textDim);
    Path arrow;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
    arrow.lineTo(arrowX, arrowY + 2.5f);
    arrow.lineTo(arrowX + 4.0f, arrowY - 2.0f);
    g.strokePath(arrow, PathStrokeType(1.6f, PathStrokeType::mitered, PathStrokeType::rounded));
}

Font AppLookAndFeel::getComboBoxFont(ComboBox&)
{
    return Font(FontOptions(13.0f));
}

void AppLookAndFeel::positionComboBoxText(ComboBox& box, Label& label)
{
    label.setJustificationType(Justification::centredLeft);
    label.setBounds(10, 0, jmax(8, box.getWidth() - 32), box.getHeight());
    label.setFont(getComboBoxFont(box));
}

void AppLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int w, int h,
                                              float sliderPos, float, float,
                                              Slider::SliderStyle, Slider&)
{
    auto bounds = Rectangle<float>((float)x, (float)y, (float)w, (float)h);
    float trackY = bounds.getCentreY();

    g.setColour(FPColours::buttonActive);
    g.fillRoundedRectangle(bounds.getX(), trackY - 3.0f, bounds.getWidth(), 6.0f, 3.0f);

    g.setColour(FPColours::accent);
    g.fillRoundedRectangle(bounds.getX(), trackY - 3.0f, sliderPos - bounds.getX(), 6.0f, 3.0f);

    g.setColour(FPColours::text);
    g.fillEllipse(sliderPos - 7.0f, trackY - 7.0f, 14.0f, 14.0f);
}
