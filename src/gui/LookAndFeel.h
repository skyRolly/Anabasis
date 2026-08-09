// Provenance (ADR-0009): adapted from Anamorph src/gui/LookAndFeel.h:1-176 @ b6a3db8.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abgui
{

// ============================================================================
//  Palette + LookAndFeel
//
//  A clean, premium "digital plugin" aesthetic (spec section 10): near-black
//  background, a restrained cool accent gradient, modern thin-arc knobs, no
//  skeuomorphism (no wood, brushed metal or vintage VU meters).
// ============================================================================
namespace colours
{
    const juce::Colour bg        { 0xff0e1014 };
    const juce::Colour bgPanel   { 0xff161a21 };
    const juce::Colour bgRaised  { 0xff1d222b };
    const juce::Colour outline   { 0xff2a313d };
    const juce::Colour text      { 0xffd7dde6 };
    const juce::Colour textDim   { 0xff8b94a3 };
    // ⊕ Anabasis accent family — DESIGN §6.1's warm amber/gold direction,
    // exact values chosen at P5 as that section instructs; the swatch awaits
    // owner ratification (C8-adjacent product identity). The neutral roles
    // above are the FAMILY palette, reused verbatim. `warn` moves off amber
    // (Anamorph's warn is inside this accent family) to a desaturated red so
    // over-ceiling states stay distinguishable from ordinary accent fills —
    // including under deuteranopia, where gold-vs-red survives as a
    // luminance difference (the §8 colour-blind-safe requirement).
    const juce::Colour accent    { 0xfff0b432 }; // gold
    const juce::Colour accent2   { 0xffe07830 }; // amber/copper
    const juce::Colour warn      { 0xffd96a5a };
}

// ============================================================================
//  Glass surfaces (feedback #17)
//
//  A subtle, reversible "iOS-26 liquid glass" treatment shared by every framed
//  surface (scope, meters, panels): a diagonal micro-gradient that is brightest
//  at the TOP-RIGHT and darkest at the BOTTOM-LEFT, plus soft highlight edges on
//  the top-left and bottom-right so the frame reads like a pane of glass. Kept
//  deliberately faint so it never overpowers the existing dark aesthetic.
// ============================================================================
namespace glass
{
    // Highlight edges + base hairline only (the caller fills the interior). The
    // top-left corner catches the brightest, thickest highlight; the bottom-right
    // a dimmer one; the other two corners stay un-lit for diagonal contrast, and
    // a soft inset stroke blends the bright edge into the content.
    void drawEdges (juce::Graphics&, juce::Rectangle<float> bounds, float radius,
                    float strength = 1.0f);
    // Diagonal depth gradient (top-right bright -> bottom-left dark) + glass edges.
    void fillPanel (juce::Graphics&, juce::Rectangle<float> bounds, float radius,
                    juce::Colour base, float strength = 1.0f);
    // Glass rim for round controls: a bright top-left arc with a faint glow on the
    // opposite edge, matching the panel edges (#16).
    void drawCircleEdge (juce::Graphics&, float centreX, float centreY, float radius,
                         float strength = 1.0f);
}

// Eased 0..1 animation property ("hovA"/"actA"/"onA") published by the editor's
// micro-anim driver (F3). Falls back to the binary state for components that
// aren't registered (or before the first animated frame), so every drawing path
// works with or without the driver.
inline float animOr (const juce::Component& c, const char* key, bool fallback)
{
    if (const auto* v = c.getProperties().getVarPointer (key))
        return (float) (double) *v;
    return fallback ? 1.0f : 0.0f;
}

class AnabasisLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnabasisLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    // Inset the interactive track by a thumb-radius so the thumb stays fully on the
    // track AND tracks the cursor 1:1 (no lag), without a remap that desynced them
    // (#4/#5).
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    // Indent the selected text a little from the left edge (#13).
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // Honour each Label's explicitly-set font instead of forcing one size, so the
    // larger Simple-mode Widen text actually renders (recurring font request).
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // Glassy highlight on the hovered pop-up row (Apple "liquid glass", #6).
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    // Unify the pop-up list with the rounded flat-design of the combo box (#22).
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    // Small dim caps header for the preset menu's FACTORY / USER sections (F2).
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override;
    int  getPopupMenuBorderSize() override { return 3; } // narrower top/bottom dead-zone (#9)
    // Fixed, uniform row height so a taller combo doesn't get taller rows (#3).
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardHeight, int& idealWidth, int& idealHeight) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // A value box you can drag (up/down) to change the value, like the knob (#2).
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Focused text fields tagged with a "glow" property get the combo's subtle
    // accent micro-glow instead of a plain hard outline (#11).
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    // Uniform, compact font for every combo + its pop-up list (#13).
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    // Drop the combo pop-up BELOW the box (target its screen bounds) instead of the JUCE default,
    // which covers the box with the currently-selected item under the cursor. Restores the expected
    // drop-down position. (#combo)
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;

    // Styled tooltip to match the design language (no system tooltip, #20).
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos,
                                           juce::Rectangle<int> parentArea) override;

    // The same contract every other class in `src/gui` carries (`CurveView`,
    // `GrHistoryView`, `SpectrumView`, `LoudnessMeterView`, `FrameClock`, the
    // editor itself): non-copyable, and leak-checked in debug builds. Restored
    // at review round 58 — it was lost when this class was rewritten for
    // Anabasis, leaving the one member of the new GUI set that was silently
    // copyable and untracked. `AnabasisAudioProcessorEditor` holds a single
    // `lnf` by value and `setLookAndFeel` takes a pointer, so nothing copies
    // one; the macro states that rather than leaving it to hold by accident.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisLookAndFeel)
};

// ============================================================================
//  The graph-well mode switch (0.1.1, owner directive; reworked 0.1.2 items
//  4+5) — ONE definition for the two views that share the well. A two-segment
//  rounded pill in the well's BOTTOM-LEFT corner showing BOTH modes
//  ("GR | SPEC" — GR first, the default mode), the active segment
//  highlighted, the whole control translucent so what it floats over stays
//  readable. Both `SpectrumView` and `GrHistoryView` draw it LAST and key
//  `hitTest`/`mouseDown` on the same rectangle, so the two views cannot
//  drift apart in geometry or behaviour.
//
//  PLACEMENT (item 4): the top-right corner the 0.1.1 pill occupied is where
//  the GR view's NEWEST data lands — the reduction the user is actively
//  watching — and the owner reported the collision. The bottom-left is the
//  least informative corner in both modes: in GR it is the OLDEST end of the
//  waveform (and, since the item-3 fixed scale, the empty zero region until
//  a full window has played); in SPEC it sits under the low-frequency fill,
//  whose information lives on the curve's top edge.
//
//  INTERACTION (item 5): the pill is ONE TOGGLE — any press inside it
//  switches the well to the other mode. The 0.1.1 side-of-divider semantics
//  made a press on the active segment a silent no-op, which read as a stuck
//  control ("clicking SPEC does not switch back"); the segments remain as
//  the state display, not as separate targets.
// ============================================================================
namespace graph_switch
{
    inline constexpr int kW = 78, kH = 18, kInsetX = 6, kInsetY = 4;

    inline juce::Rectangle<int> bounds (int viewWidth, int viewHeight) noexcept
    {
        juce::ignoreUnused (viewWidth);
        return { kInsetX, viewHeight - kInsetY - kH, kW, kH };
    }

    // Segment order (0.1.2 item 4): GR (false, the default mode) left,
    // SPEC (true) right. Display only — see the banner's interaction note.
    inline juce::Rectangle<int> grHalf (int viewWidth, int viewHeight) noexcept
    { return bounds (viewWidth, viewHeight).removeFromLeft (kW / 2); }
    inline juce::Rectangle<int> specHalf (int viewWidth, int viewHeight) noexcept
    { return bounds (viewWidth, viewHeight).removeFromRight (kW - kW / 2); }

    inline void paint (juce::Graphics& g, int viewWidth, int viewHeight, bool spectrumActive)
    {
        const auto r = bounds (viewWidth, viewHeight).toFloat();
        const float rad = r.getHeight() * 0.5f;

        // Translucent body: present enough to read, never a solid mask over
        // the trace beneath (the owner's explicit requirement).
        g.setColour (colours::bg.withAlpha (0.55f));
        g.fillRoundedRectangle (r, rad);

        // Active-segment highlight, clipped to the pill so the highlight's
        // outer corners follow the rounded outline.
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, rad);
            g.reduceClipRegion (clip);
            const auto seg = (spectrumActive ? specHalf (viewWidth, viewHeight)
                                             : grHalf (viewWidth, viewHeight)).toFloat();
            g.setColour (colours::accent.withAlpha (0.26f));
            g.fillRect (seg);
        }

        g.setColour (colours::outline.withAlpha (0.85f));
        g.drawRoundedRectangle (r, rad, 1.0f);
        // The divider between the two mode labels.
        g.drawVerticalLine (bounds (viewWidth, viewHeight).getX() + kW / 2,
                            r.getY() + 3.0f, r.getBottom() - 3.0f);

        g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.08f));
        g.setColour (spectrumActive ? colours::textDim : colours::accent);
        g.drawText ("GR", grHalf (viewWidth, viewHeight), juce::Justification::centred);
        g.setColour (spectrumActive ? colours::accent : colours::textDim);
        g.drawText ("SPEC", specHalf (viewWidth, viewHeight), juce::Justification::centred);
    }
} // namespace graph_switch

// `CompactComboLookAndFeel` and `SimpleComboLookAndFeel` used to sit here — two
// `AnabasisLookAndFeel` subclasses varying the combo font and pop-up row
// height. Removed at review round 53 as unported migration state, and the
// evidence was in their own comments: they named the controls they were "applied
// only to" as the compact Input Channel / M/S Solo combos and the two
// Simple-mode Widen combos (algorithm + Style/Focus). Those are the SIBLING
// product's controls. Anabasis has no Widen stage, no input-channel selector and
// no M/S solo, nothing in `src/gui` ever instantiated either class, and the
// editor holds one `abgui::AnabasisLookAndFeel lnf` for every combo in the tree.
// Left standing they invited a reader to assume a compact/large combo variant
// was wired and to style a new control by "just using the existing variant" —
// the same trap `allCombos`/`hov` (round 28) and `resetSweep` (round 43) were
// removed for. A genuine size variant, if the §6.2/§6.3 layout ever wants one,
// is a five-line subclass written against the control that needs it.

} // namespace abgui
