#pragma once
#include <JuceHeader.h>

namespace FPColours
{
    const Colour background      { 0xffebebf0 };
    const Colour surface         { 0xffffffff };
    const Colour surfaceLight    { 0xfff4f4f7 };
    const Colour rowFill         { 0xfffafafc };
    const Colour rowFillAlt      { 0xffeef0f4 };
    const Colour gridLine        { 0xffcfcfd8 };
    const Colour gridLineBright  { 0xffa8a8b4 };
    const Colour text            { 0xff1b1b23 };
    const Colour textDim         { 0xff6e6e7a };
    const Colour pitchCurve      { 0xff1b1b23 };
    const Colour pitchCurveAlt   { 0xff3f3f4c };
    const Colour vibrato         { 0xff3a3a46 };
    const Colour accent          { 0xff3a3a46 };
    const Colour accentCyan      { 0xff5a5a66 };
    const Colour toolbarBg       { 0xffe3e3ea };
    const Colour buttonHover     { 0xfff2f2f6 };
    const Colour buttonActive    { 0xffd8d8e0 };
    const Colour playhead        { 0xff111116 };
    const Colour pianoWhiteKey   { 0xffffffff };

    inline int relativeInterval(int midiNote, int rootNote)
    {
        return ((midiNote - rootNote) % 12 + 12) % 12;
    }

    inline Colour swaraColour(int relInterval)
    {
        int r = relativeInterval(relInterval, 0);
        if (r == 0)  return Colour(0xffd64533);
        if (r <= 2)  return Colour(0xffe07b2f);
        if (r <= 4)  return Colour(0xffd9ab26);
        if (r <= 6)  return Colour(0xff4aa35a);
        if (r == 7)  return Colour(0xff3f6fd0);
        if (r <= 9)  return Colour(0xff5046c4);
        return Colour(0xff9b4fc0);
    }

    inline String swaraName(int relInterval)
    {
        int r = relativeInterval(relInterval, 0);
        if (r == 0)  return "Sa";
        if (r <= 2)  return "Ri";
        if (r <= 4)  return "Ga";
        if (r <= 6)  return "Ma";
        if (r == 7)  return "Pa";
        if (r <= 9)  return "Dha";
        return "Ni";
    }
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
    Font getComboBoxFont(ComboBox&) override;
    void positionComboBoxText(ComboBox&, Label&) override;

    void drawLinearSlider(Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          Slider::SliderStyle, Slider&) override;
};
