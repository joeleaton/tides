#pragma once
#include <juce_graphics/juce_graphics.h>

// Design tokens lifted from design_handoff_tides/tides.css ("Lab" palette).
// Names mirror the CSS custom-prop names so the design and code stay in sync.
namespace tides::ui
{
    // ---- Colour tokens --------------------------------------------------
    namespace col
    {
        // Surface
        inline const juce::Colour Bg          { 0xff050608 }; // editor surround
        inline const juce::Colour BgDeep      { 0xff020204 }; // recessed scope bg
        inline const juce::Colour Panel       { 0xff1a1b20 }; // pane surface
        inline const juce::Colour PanelHi     { 0xff23242a }; // raised in panes
        inline const juce::Colour PanelLo     { 0xff0d0e11 }; // knob faces, inputs

        // Dividers / edges
        inline const juce::Colour DividerGrout = juce::Colour::fromFloatRGBA (0.0f, 0.0f, 0.0f, 0.60f);
        inline const juce::Colour Divider2     = juce::Colour::fromFloatRGBA (0.863f, 0.902f, 0.961f, 0.18f);
        inline const juce::Colour EdgeHi       = juce::Colour::fromFloatRGBA (0.863f, 0.902f, 0.961f, 0.06f);

        // Ink
        inline const juce::Colour Ink         { 0xffeff2f7 };
        inline const juce::Colour InkMid      { 0xffc2c6d0 };
        inline const juce::Colour InkLo       { 0xff8e94a0 };
        inline const juce::Colour InkFaint    { 0xff4a4f59 };

        // Primary / secondary accent (legacy name 'amber' carries the cyan value)
        inline const juce::Colour Amber       { 0xff7ad7ff };
        inline const juce::Colour AmberHot    { 0xffb8e9ff };
        inline const juce::Colour Purple      { 0xffa986ff };
        inline const juce::Colour PurpleHot   { 0xffc7afff };

        // Polarity (bipolar attenuverters)
        inline const juce::Colour Pos         { 0xffff6a5a };
        inline const juce::Colour Neg         { 0xff6db4ff };

        // Knob materials
        inline const juce::Colour Knob        { 0xff232429 };
        inline const juce::Colour KnobHi      { 0xff34363d };
        inline const juce::Colour KnobEdge    = juce::Colour::fromFloatRGBA (0.863f, 0.902f, 0.961f, 0.14f);

        inline const juce::Colour RingBase    = juce::Colour::fromFloatRGBA (0.863f, 0.902f, 0.961f, 0.10f);

        // Per-group hues (Voices grid, focused chip border)
        inline const juce::Colour GroupHues[6] = {
            juce::Colour (0xff7ad7ff), // G1 cyan
            juce::Colour (0xffa986ff), // G2 purple
            juce::Colour (0xffffb547), // G3 amber
            juce::Colour (0xff5cdfb0), // G4 mint
            juce::Colour (0xffff8aa3), // G5 rose
            juce::Colour (0xff9ab0ff), // G6 lilac
        };
    }

    // ---- Dimensions -----------------------------------------------------
    namespace dim
    {
        constexpr int kEditorW   = 1280;
        constexpr int kEditorH   = 800;
        constexpr int kCornerR   = 14;
        constexpr int kGrout     = 2;

        constexpr int kTopBarH   = 44;
        constexpr int kGroupBarH = 56;
        constexpr int kVizStripH = 128;
        constexpr int kPerfRowH  = 64;
        constexpr int kMainH     = kEditorH - kTopBarH - kGroupBarH - kVizStripH - kPerfRowH;
        constexpr int kMainTopH  = 280;
        constexpr int kMainBotH  = kMainH - kMainTopH - kGrout;

        // Top row
        constexpr int kExciterW   = 320;
        constexpr int kOutputW    = 312;
        // Bottom row
        constexpr int kEnvelopeW  = 240;
        constexpr int kQuadLFOW   = 420;
        // Viz row
        constexpr int kScopeW     = 600;
        constexpr int kMetersW    = 220;

        // Pane padding (per design)
        constexpr int kPanePadTop   = 9;
        constexpr int kPanePadSide  = 12;
        constexpr int kPanePadBot   = 8;

        // Knob diameters
        constexpr int kKnobS  = 40;
        constexpr int kKnobM  = 50;
        constexpr int kKnobL  = 62;
        constexpr int kKnobXL = 76;
    }

    // ---- Font helpers ---------------------------------------------------
    // tracking == JUCE's extraKerningFactor, a multiplier of font height.
    // CSS letter-spacing 0.12 em ≈ kerning factor 0.12.
    namespace font
    {
        inline juce::Font mono (float height, float tracking = 0.12f, bool bold = false)
        {
            juce::FontOptions opt { "JetBrains Mono", height,
                                    bold ? juce::Font::bold : juce::Font::plain };
            return juce::Font (opt).withExtraKerningFactor (tracking);
        }

        inline juce::Font sans (float height, bool semiBold = false, float tracking = 0.0f)
        {
            juce::FontOptions opt { "Instrument Sans", height,
                                    semiBold ? juce::Font::bold : juce::Font::plain };
            return juce::Font (opt).withExtraKerningFactor (tracking);
        }
    }
}
