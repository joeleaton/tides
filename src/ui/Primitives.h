#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <vector>
#include "ui/Theme.h"

namespace tides::ui
{
    // ---- Switch ---------------------------------------------------------
    // Pill toggle with a leading dot and uppercase mono label.
    class Switch : public juce::Component
    {
    public:
        explicit Switch (juce::String labelText) : label (std::move (labelText))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void setValue (bool v, bool notify = true)
        {
            if (v == value) return;
            value = v;
            if (notify && onChange) onChange (v);
            repaint();
        }
        bool getValue() const { return value; }

        std::function<void (bool)> onChange;

        void mouseDown (const juce::MouseEvent&) override { setValue (! value); }
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto rf = getLocalBounds().toFloat();
            // Background
            g.setColour (col::PanelLo);
            g.fillRoundedRectangle (rf, 4.0f);
            // Border
            g.setColour (hover ? col::Divider2 : col::DividerGrout);
            g.drawRoundedRectangle (rf.reduced (0.5f), 4.0f, 1.0f);
            // Inset top hi
            g.setColour (col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            // Dot
            const float dotR = 3.5f;
            const float dx = 8.0f;
            const float dy = rf.getHeight() * 0.5f - dotR;
            if (value)
            {
                g.setColour (col::Amber.withAlpha (0.55f));
                g.fillEllipse (dx - 2.0f, dy - 2.0f, dotR * 2.0f + 4.0f, dotR * 2.0f + 4.0f);
                g.setColour (col::Amber);
            }
            else
            {
                g.setColour (col::InkFaint);
            }
            g.fillEllipse (dx, dy, dotR * 2.0f, dotR * 2.0f);

            // Label
            g.setColour (value ? col::Ink : col::InkMid);
            g.setFont (font::mono (9.0f, 0.10f));
            g.drawText (label.toUpperCase(),
                        juce::Rectangle<int> (22, 0, getWidth() - 28, getHeight()),
                        juce::Justification::centredLeft, false);
        }

    private:
        juce::String label;
        bool value { false }, hover { false };
    };

    // ---- Segmented control (mono labels) -------------------------------
    class Seg : public juce::Component
    {
    public:
        struct Option { juce::String label; int value; };

        explicit Seg (std::vector<Option> opts) : options (std::move (opts)) {}

        void setSelectedValue (int v, bool notify = true)
        {
            if (v == current) return;
            current = v;
            if (notify && onChange) onChange (v);
            repaint();
        }
        int getSelectedValue() const { return current; }

        std::function<void (int)> onChange;

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int n = (int) options.size();
            if (n == 0) return;
            const int idx = juce::jlimit (0, n - 1, (int) (e.position.x / (float) getWidth() * (float) n));
            setSelectedValue (options[(size_t) idx].value);
        }

        void paint (juce::Graphics& g) override
        {
            auto rf = getLocalBounds().toFloat();
            // Background
            g.setColour (col::PanelLo);
            g.fillRoundedRectangle (rf, 5.0f);
            g.setColour (col::DividerGrout);
            g.drawRoundedRectangle (rf.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            const int n = (int) options.size();
            if (n == 0) return;
            const float cellW = (float) getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                auto cell = juce::Rectangle<float> (i * cellW, 0.0f, cellW, (float) getHeight()).reduced (2.0f);
                const bool on = options[(size_t) i].value == current;
                if (on)
                {
                    g.setColour (col::Ink);
                    g.fillRoundedRectangle (cell, 3.0f);
                }
                g.setColour (on ? col::Bg : col::InkMid);
                g.setFont (font::mono (9.0f, 0.08f));
                g.drawText (options[(size_t) i].label.toUpperCase(),
                            cell.toNearestInt(),
                            juce::Justification::centred, false);
            }
        }

    private:
        std::vector<Option> options;
        int current { 0 };
    };

    // ---- MiniSlider (compact horizontal 0..1 control) ------------------
    // Used in the perf row for DET / SPR. Click-and-drag to set; a small
    // value readout sits to the right of the bar.
    class MiniSlider : public juce::Component
    {
    public:
        MiniSlider() { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }

        void setValue (float v, bool notify = true)
        {
            v = juce::jlimit (0.0f, 1.0f, v);
            if (juce::approximatelyEqual (v, value)) return;
            value = v;
            if (notify && onChange) onChange (v);
            repaint();
        }
        float getValue() const { return value; }
        void setFormatter (std::function<juce::String (float)> f) { fmt = std::move (f); repaint(); }

        std::function<void (float)> onChange;

        void mouseDown (const juce::MouseEvent& e) override { setFromMouse (e); }
        void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }

        void paint (juce::Graphics& g) override
        {
            const int valueW = 36;
            auto bar = getLocalBounds().withTrimmedRight (valueW + 2);
            auto rf  = bar.toFloat();

            g.setColour (col::PanelLo);
            g.fillRoundedRectangle (rf, 3.0f);
            g.setColour (col::DividerGrout);
            g.drawRoundedRectangle (rf.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            // Fill from left
            const float fillW = rf.getWidth() * value;
            if (fillW > 1.0f)
            {
                juce::ColourGradient grad (col::Amber.withAlpha (0.20f), rf.getX(), rf.getY(),
                                           col::Amber.withAlpha (0.60f), rf.getRight(), rf.getY(),
                                           false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (juce::Rectangle<float> (rf.getX(), rf.getY(), fillW, rf.getHeight())
                                            .reduced (1.0f, 1.0f),
                                        2.0f);
            }

            // Value readout
            g.setColour (col::Ink);
            g.setFont (font::mono (9.0f, 0.0f));
            const auto txt = fmt ? fmt (value) : juce::String (value, 2);
            g.drawText (txt,
                        juce::Rectangle<int> (getWidth() - valueW, 0, valueW, getHeight()),
                        juce::Justification::centredLeft, false);
        }

    private:
        void setFromMouse (const juce::MouseEvent& e)
        {
            const int valueW = 36;
            const float barW = (float) (getWidth() - valueW - 2);
            if (barW <= 0.0f) return;
            const float v = juce::jlimit (0.0f, 1.0f, (float) e.x / barW);
            setValue (v);
        }

        float value { 0.0f };
        std::function<juce::String (float)> fmt;
    };

    // ---- Stepper for integers -----------------------------------------
    class Stepper : public juce::Component
    {
    public:
        Stepper (int lo, int hi) : minV (lo), maxV (hi) {}

        void setValue (int v, bool notify = true)
        {
            v = juce::jlimit (minV, maxV, v);
            if (v == value) return;
            value = v;
            if (notify && onChange) onChange (v);
            repaint();
        }
        int getValue() const { return value; }
        void setFormatter (std::function<juce::String (int)> f) { fmt = std::move (f); repaint(); }

        std::function<void (int)> onChange;

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int btnW = 18;
            if (e.position.x < (float) btnW)            setValue (value - 1);
            else if (e.position.x > (float) (getWidth() - btnW)) setValue (value + 1);
        }

        void paint (juce::Graphics& g) override
        {
            auto rf = getLocalBounds().toFloat();
            g.setColour (col::PanelLo);
            g.fillRoundedRectangle (rf, 4.0f);
            g.setColour (col::DividerGrout);
            g.drawRoundedRectangle (rf.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            const int btnW = 18;
            const auto rl = juce::Rectangle<int> (0, 0, btnW, getHeight());
            const auto rr = juce::Rectangle<int> (getWidth() - btnW, 0, btnW, getHeight());
            const auto rm = juce::Rectangle<int> (btnW, 0, getWidth() - btnW * 2, getHeight());

            g.setColour (col::InkMid);
            g.setFont (font::mono (11.0f, 0.0f));
            g.drawText (juce::String::fromUTF8 ("−"), rl, juce::Justification::centred, false);
            g.drawText ("+", rr, juce::Justification::centred, false);

            g.setColour (col::Ink);
            g.setFont (font::mono (10.0f, 0.0f));
            const auto text = fmt ? fmt (value) : juce::String (value);
            g.drawText (text, rm, juce::Justification::centred, false);
        }

    private:
        int minV, maxV;
        int value { 0 };
        std::function<juce::String (int)> fmt;
    };
}
