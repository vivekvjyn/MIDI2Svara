#include "LookAndFeel.h"


AppLookAndFeel::AppLookAndFeel()
{
    setColour(ResizableWindow::backgroundColourId, FPColours::background);
    setColour(TextButton::buttonColourId, FPColours::surface);
    setColour(TextButton::textColourOffId, FPColours::text);
    setColour(ComboBox::backgroundColourId, FPColours::surface);
    setColour(ComboBox::textColourId, FPColours::text);
    setColour(ComboBox::outlineColourId, FPColours::gridLine);
    setColour(Slider::backgroundColourId, FPColours::surface);
    setColour(Slider::trackColourId, FPColours::accent);
    setColour(Slider::thumbColourId, FPColours::text);
    setColour(Label::textColourId, FPColours::text);
    setColour(PopupMenu::backgroundColourId, FPColours::surface);
    setColour(PopupMenu::textColourId, FPColours::text);
    setColour(PopupMenu::highlightedBackgroundColourId, FPColours::buttonActive);
}

void AppLookAndFeel::drawButtonBackground(Graphics& g, Button& button,
                                                   const Colour&,
                                                   bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto colour = FPColours::surface;

    if (button.getToggleState())
        colour = FPColours::accent.withAlpha(0.3f);
    else if (isButtonDown)
        colour = FPColours::buttonActive;
    else if (isMouseOver)
        colour = FPColours::buttonHover;

    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(FPColours::gridLine);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void AppLookAndFeel::drawButtonText(Graphics& g, TextButton& button,
                                            bool , bool )
{
    auto colour = button.getToggleState() ? FPColours::accent : FPColours::text;
    g.setColour(colour);
    g.setFont(FontOptions(13.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds(), Justification::centred);
}

void AppLookAndFeel::drawComboBox(Graphics& g, int width, int height, bool ,
                                          int, int, int, int, ComboBox& )
{
    auto bounds = Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
    g.setColour(FPColours::surface);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(FPColours::gridLine);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    auto arrowZone = Rectangle<float>((float)width - 20.0f, 0, 20.0f, (float)height);
    Path arrow;
    arrow.addTriangle(arrowZone.getCentreX() - 4, arrowZone.getCentreY() - 2,
                      arrowZone.getCentreX() + 4, arrowZone.getCentreY() - 2,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
    g.setColour(FPColours::textDim);
    g.fillPath(arrow);
}

void AppLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int w, int h,
                                              float sliderPos, float, float,
                                              Slider::SliderStyle, Slider&)
{
    auto bounds = Rectangle<float>((float)x, (float)y, (float)w, (float)h);
    float trackY = bounds.getCentreY();

    g.setColour(FPColours::surface);
    g.fillRoundedRectangle(bounds.getX(), trackY - 2, bounds.getWidth(), 4.0f, 2.0f);

    g.setColour(FPColours::accent);
    g.fillRoundedRectangle(bounds.getX(), trackY - 2, sliderPos - bounds.getX(), 4.0f, 2.0f);

    g.setColour(FPColours::text);
    g.fillEllipse(sliderPos - 5, trackY - 5, 10, 10);
}

void AppLookAndFeel::drawLabel(Graphics& g, Label& label)
{
    g.setColour(label.findColour(Label::textColourId));
    g.setFont(getLabelFont(label));
    g.drawText(label.getText(), label.getLocalBounds(), label.getJustificationType());
}

Font AppLookAndFeel::getLabelFont(Label&)
{
    return Font(FontOptions(12.0f));
}
