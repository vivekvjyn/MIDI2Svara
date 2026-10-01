#pragma once
#include <JuceHeader.h>

namespace FPColours
{
    
    const Colour background      { 0xff0f0d14 }; 
    const Colour surface         { 0xff1a1520 }; 
    const Colour surfaceLight    { 0xff2a2435 }; 
    const Colour gridLine        { 0xff2e2838 }; 
    const Colour gridLineBright  { 0xff3e384a }; 
    const Colour text            { 0xfff0e8d8 }; 
    const Colour textDim         { 0xff908878 }; 
    const Colour noteBlock       { 0xffd4a030 }; 
    const Colour noteBlockSelected { 0xffe8b838 }; 
    const Colour pitchCurve      { 0xffc040a0 }; 
    const Colour peacockBlue     { 0xff3080b0 }; 
    const Colour pitchCurveAlt   { 0xff3080b0 }; 
    const Colour vibrato         { 0xff40b080 }; 
    const Colour gamaka          { 0xffd05828 }; 
    const Colour accent          { 0xffc040a0 }; 
    const Colour accentCyan      { 0xff3080b0 }; 
    const Colour toolbarBg       { 0xff151018 }; 
    const Colour buttonHover     { 0xff2e2840 }; 
    const Colour buttonActive    { 0xff3e3850 }; 
    const Colour playhead        { 0xffd04030 }; 
    const Colour pianoWhiteKey   { 0xff1e1828 }; 
    const Colour pianoBlackKey   { 0xff0f0d14 }; 
    const Colour pianoKeyBorder  { 0xff2e2838 }; 
    const Colour settingsBg      { 0xff18131e };
    const Colour settingsBorder  { 0xff2e2838 };
}

class AppLookAndFeel : public LookAndFeel_V4
{
public:
    AppLookAndFeel();

    void drawButtonBackground(Graphics&, Button&, const Colour&,
                              bool isMouseOver, bool isButtonDown) override;
    void drawButtonText(Graphics&, TextButton&, bool isMouseOver, bool isButtonDown) override;

    void drawComboBox(Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, ComboBox&) override;

    void drawLinearSlider(Graphics&, int x, int y, int w, int h,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          Slider::SliderStyle, Slider&) override;

    void drawLabel(Graphics&, Label&) override;

    Font getLabelFont(Label&) override;
};
