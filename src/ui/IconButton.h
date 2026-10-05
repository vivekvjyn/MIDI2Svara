#pragma once
#include <JuceHeader.h>
#include "LookAndFeel.h"

class IconButton : public Button
{
public:
    enum class Icon { Move, Pencil, Refresh };

    IconButton(const String& accessibilityName, Icon initialIcon)
        : Button(accessibilityName), icon(initialIcon) {}

    void paintButton(Graphics& g, bool, bool) override
    {
        getLookAndFeel().drawButtonBackground(g, *this,
                                              findColour(TextButton::buttonColourId),
                                              isMouseOver(), isDown());

        auto c = getToggleState() ? FPColours::surface : FPColours::text;

        drawIcon(g, icon, getLocalBounds().toFloat().reduced(6.0f), c);
    }

    static void drawIcon(Graphics& g, Icon icon, Rectangle<float> r, const Colour& colour)
    {
        if (r.getWidth() <= 1.0f || r.getHeight() <= 1.0f) return;

        auto X = [&](float u) { return r.getX() + u * r.getWidth(); };
        auto Y = [&](float v) { return r.getY() + v * r.getHeight(); };

        g.setColour(colour);
        Path p;

        switch (icon)
        {
            case Icon::Move:
                p.startNewSubPath(X(0.50f), Y(0.10f));
                p.lineTo(X(0.50f), Y(0.90f));
                p.startNewSubPath(X(0.10f), Y(0.50f));
                p.lineTo(X(0.90f), Y(0.50f));
                g.strokePath(p, PathStrokeType(1.8f, PathStrokeType::mitered, PathStrokeType::rounded));

                {
                    Path heads;
                    heads.addTriangle(X(0.50f), Y(0.02f), X(0.34f), Y(0.24f), X(0.66f), Y(0.24f));
                    heads.addTriangle(X(0.50f), Y(0.98f), X(0.34f), Y(0.76f), X(0.66f), Y(0.76f));
                    heads.addTriangle(X(0.02f), Y(0.50f), X(0.24f), Y(0.34f), X(0.24f), Y(0.66f));
                    heads.addTriangle(X(0.98f), Y(0.50f), X(0.76f), Y(0.34f), X(0.76f), Y(0.66f));
                    g.fillPath(heads);
                }
                break;

            case Icon::Pencil:
            {
                Path body;
                body.addQuadrilateral(X(0.330f), Y(0.760f),
                                      X(0.830f), Y(0.260f),
                                      X(0.740f), Y(0.170f),
                                      X(0.240f), Y(0.670f));
                g.fillPath(body);

                Path tip;
                tip.addTriangle(X(0.110f), Y(0.890f),
                                X(0.330f), Y(0.760f),
                                X(0.240f), Y(0.670f));
                g.fillPath(tip);

                p.startNewSubPath(X(0.110f), Y(0.890f));
                p.lineTo(X(0.240f), Y(0.670f));
                g.strokePath(p, PathStrokeType(1.4f, PathStrokeType::mitered, PathStrokeType::rounded));
                break;
            }

            case Icon::Refresh:
            {
                const float cx = r.getCentreX();
                const float cy = r.getCentreY();
                const float rad = r.getWidth() * 0.34f;
                const float startA = 0.35f * MathConstants<float>::pi;
                const float endA   = 2.10f * MathConstants<float>::pi;

                p.addCentredArc(cx, cy, rad, rad, 0.0f, startA, endA, true);
                g.strokePath(p, PathStrokeType(1.9f, PathStrokeType::curved, PathStrokeType::rounded));

                const float sx = cx + std::cos(startA) * rad;
                const float sy = cy + std::sin(startA) * rad;
                const float tx = -std::sin(startA);
                const float ty =  std::cos(startA);
                const float sz = r.getWidth() * 0.17f;

                Path head;
                head.addTriangle(sx + tx * sz, sy + ty * sz,
                                 sx - tx * sz * 0.5f - ty * sz * 0.7f,
                                 sy - ty * sz * 0.5f + tx * sz * 0.7f,
                                 sx - tx * sz * 0.5f + ty * sz * 0.7f,
                                 sy - ty * sz * 0.5f - tx * sz * 0.7f);
                g.fillPath(head);
                break;
            }
        }
    }

private:
    Icon icon;
};
