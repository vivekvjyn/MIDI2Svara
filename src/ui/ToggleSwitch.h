#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"

class ToggleSwitch : public Button
{
public:
    ToggleSwitch() : Button("MPE output") {}

    void paintButton(Graphics& g, bool, bool) override
    {
        auto area = getLocalBounds().toFloat();

        const float trackW = 34.0f;
        const float trackH = jmin(18.0f, area.getHeight() - 6.0f);
        auto track = Rectangle<float>(area.getRight() - trackW,
                                      area.getCentreY() - trackH * 0.5f,
                                      trackW, trackH);

        g.setColour(FPColours::text);
        g.setFont(FontOptions(12.0f));
        g.drawText("MPE", Rectangle<float>(area.getX(), track.getY(),
                                           track.getX() - area.getX() - 6.0f,
                                           track.getHeight()),
                   Justification::centredRight);

        float corner = track.getHeight() * 0.5f;

        g.setColour(getToggleState() ? FPColours::text : FPColours::buttonActive);
        g.fillRoundedRectangle(track, corner);

        float knobD = track.getHeight() - 4.0f;
        float knobX = getToggleState() ? (track.getRight() - knobD - 2.0f)
                                       : (track.getX() + 2.0f);
        g.setColour(FPColours::surface);
        g.fillEllipse(knobX, track.getY() + 2.0f, knobD, knobD);
    }
};
