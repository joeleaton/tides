#pragma once
#include <JuceHeader.h>
#include <functional>
#include "ui/Theme.h"

namespace tides::ui
{
    // ---- PillButton -----------------------------------------------------
    // Small uppercase-mono button matching the design's .btn style:
    // 28 px tall, padding 0 10, PanelLo bg, Divider border, 5 px round.
    // Hover brightens the border + ink; "active" highlight uses cyan tint.
    class PillButton : public juce::Component
    {
    public:
        explicit PillButton (juce::String t = {}) : text (std::move (t))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void setText (juce::String t) { text = std::move (t); repaint(); }
        void setActive (bool a)       { active = a; repaint(); }
        void setEnabledLook (bool e)  { enabledLook = e; repaint(); }

        std::function<void()> onClick;

        void mouseDown (const juce::MouseEvent&) override
        {
            if (enabledLook && onClick) onClick();
        }
        void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();

            // Background
            juce::Colour bg = col::PanelLo;
            if (active)    bg = juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.08f);
            g.setColour (bg);
            g.fillRoundedRectangle (r, 5.0f);

            // Border
            juce::Colour border = col::DividerGrout;   // dark grout-like
            if (active)      border = juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.40f);
            else if (hover)  border = col::Divider2;
            g.setColour (border);
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

            // Inset shadow (subtle)
            g.setColour (col::EdgeHi);
            g.fillRect (r.reduced (1.0f).withHeight (1.0f));

            // Text
            juce::Colour ink = col::InkMid;
            if (active)      ink = col::Amber;
            else if (hover)  ink = col::Ink;
            if (! enabledLook) ink = col::InkFaint;

            g.setColour (ink);
            g.setFont (font::mono (9.5f, 0.12f));
            g.drawText (text.toUpperCase(), getLocalBounds(),
                        juce::Justification::centred, false);
        }

    private:
        juce::String text;
        bool hover { false }, active { false }, enabledLook { true };
    };

    // ---- ABButton -------------------------------------------------------
    // Two-cell segmented button. The inverted side is the current state:
    //   showingSaved = false  -> A side inverted (Ink bg, Bg text)
    //   showingSaved = true   -> B side inverted, glowing cyan
    class ABButton : public juce::Component
    {
    public:
        ABButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void setShowingSaved (bool b) { if (b != showingSaved) { showingSaved = b; repaint(); } }
        bool getShowingSaved() const  { return showingSaved; }

        std::function<void()> onClick;

        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
        void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();

            // Outer rounded background
            g.setColour (col::PanelLo);
            g.fillRoundedRectangle (r, 5.0f);

            // Border
            juce::Colour border = showingSaved
                ? juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.60f)
                : (hover ? col::Divider2 : col::DividerGrout);
            g.setColour (border);
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

            // Halves
            const auto halfW = r.getWidth() * 0.5f;
            auto leftHalf  = r.withWidth (halfW);
            auto rightHalf = r.withTrimmedLeft (halfW);

            auto drawHalf = [&] (juce::Rectangle<float> half, const char* letter, bool isOn)
            {
                if (isOn)
                {
                    juce::Path p;
                    // Round only the corners that are on the outer edge of the half
                    if (half.getX() <= r.getX() + 0.5f)
                        p.addRoundedRectangle (half.getX(), half.getY(),
                                               half.getWidth(), half.getHeight(),
                                               5.0f, 5.0f, true, false, true, false);
                    else
                        p.addRoundedRectangle (half.getX(), half.getY(),
                                               half.getWidth(), half.getHeight(),
                                               5.0f, 5.0f, false, true, false, true);
                    g.setColour (showingSaved ? col::Amber : col::Ink);
                    g.fillPath (p);
                }
                g.setColour (isOn ? (showingSaved ? col::BgDeep : col::Bg) : col::InkLo);
                g.setFont (font::mono (10.0f, 0.10f));
                g.drawText (letter, half.toNearestInt(),
                            juce::Justification::centred, false);
            };

            drawHalf (leftHalf,  "A", ! showingSaved);
            drawHalf (rightHalf, "B",   showingSaved);

            // Cyan glow when showing saved
            if (showingSaved)
            {
                juce::DropShadow ds { col::Amber.withAlpha (0.35f), 12, {} };
                // Draw a soft outer halo via an extra stroke
                g.setColour (col::Amber.withAlpha (0.20f));
                g.drawRoundedRectangle (r.reduced (-1.0f), 6.0f, 1.5f);
            }
        }

    private:
        bool showingSaved { false }, hover { false };
    };
}
