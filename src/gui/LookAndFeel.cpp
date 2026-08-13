// Provenance (ADR-0009): adapted from Anamorph src/gui/LookAndFeel.cpp:1-912 @ b6a3db8.
#include "LookAndFeel.h"

namespace abgui
{

AnabasisLookAndFeel::AnabasisLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::bgRaised);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::PopupMenu::backgroundColourId, colours::bgPanel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::bgRaised);
    setColour (juce::TextButton::textColourOnId, colours::bg);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void AnabasisLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float startAngle, float endAngle,
                                            juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    // Draw at the EASED visual position when the micro-anim driver is publishing
    // one (preset / A-B sweep, #5); during a hand drag use the live position so
    // the pointer tracks 1:1 with no lag.
    //
    // There was a third term here — a `resetSweep` property meant to keep the
    // eased draw while a reset's button was still held — and it was REMOVED at
    // round 43 rather than wired, which is the opposite of what its own comment
    // asked for, so here is the reason. Nothing in the tree ever set it (the
    // same half-ported Anamorph state round 28 removed for `allCombos`/`hov`),
    // and setting it here would not have produced the sweep it describes: the
    // driver reaches the same conclusion one level up, where `stepMicroAnims`
    // SNAPS `vpos` straight to the target while `isMouseButtonDown()`. With the
    // source of the ease already collapsed, honouring the flag in the draw path
    // would only have drawn the un-eased value by a second route. Delivering
    // "sweep while held" needs all three sites to agree, which is new behaviour
    // and a listening-pass call — not a repair, and not something a dead read
    // was silently providing. The reachable reset gesture is unaffected either
    // way: `mouseDoubleClick` is dispatched from `internalMouseUp`, so the
    // button is already released and the ease runs.
    const bool dragging = s.isMouseButtonDown()
                       || (bool) s.getProperties().getWithDefault ("dragging", false);
    if (! dragging)
        if (const auto* v = s.getProperties().getVarPointer ("vpos"))
            pos = juce::jlimit (0.0f, 1.0f, (float) (double) *v);
    const auto angle   = startAngle + pos * (endAngle - startAngle);
    const float thick  = juce::jmax (3.0f, radius * 0.16f);

    // Interaction state (#10), now as EASED 0..1 levels from the micro-anim
    // driver (F3): hA ramps with hover, aA with press / value-number drag; hi is
    // "interacting at all". Falls back to the old binary feel when not animated.
    const bool hover  = s.isMouseOver (false);
    const bool active = s.isMouseButtonDown() || (bool) s.getProperties().getWithDefault ("dragging", false);
    const float hA = animOr (s, "hovA", hover);
    const float aA = animOr (s, "actA", active);
    const float hi = juce::jmax (hA, aA);

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, endAngle, true);
    g.setColour (colours::outline);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc: the softer palette BLUE->TEAL gradient (#1), wrapped in a MANY-
    // layered glow (faint+wide outside, bright+tight inside). Using many thin
    // layers makes the brightness falloff smooth instead of stepped/banded (#4).
    const juce::Colour arcLo (0xffe07830); // amber (accent2)
    const juce::Colour arcHi (0xfff0b432); // gold (accent)
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, angle, true);
    // The arc glow keeps its blue->teal gradient but spreads even WIDER around the
    // arc on hover / press, with many thin layers so it stays smooth (#4). The
    // eased levels make the spread glide instead of stepping (F3).
    const float glowPeak   = 0.13f + 0.11f * hi + 0.16f * aA; // idle .13 / hover .24 / press .40
    const float glowSpread = 3.4f  + 4.6f  * hi + 3.0f  * aA; // idle 3.4 / hover 8 / press 11
    constexpr int nLayers = 12;
    for (int i = 0; i < nLayers; ++i)
    {
        const float t  = (float) i / (float) (nLayers - 1); // 0 outer .. 1 inner
        const float gw = thick + (1.0f - t) * glowSpread;   // widest outside
        const float a  = glowPeak * std::pow (t, 1.5f);     // smooth brighten toward the arc
        juce::ColourGradient gg (arcLo.withAlpha (a * 0.85f), centre.x - radius, centre.y,
                                 arcHi.withAlpha (a),         centre.x + radius, centre.y, false);
        g.setGradientFill (gg);
        g.strokePath (value, juce::PathStrokeType (gw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient grad (arcLo.brighter (0.12f * aA), centre.x - radius, centre.y,
                               arcHi.brighter (0.12f * aA), centre.x + radius, centre.y, false);
    grad.addColour (0.5, arcLo.interpolatedWith (arcHi, 0.5f));
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Glassy knob face: a top-lit radial gradient over a dark base. Hover / press
    // lift the face only SLIGHTLY now -- the previous lift was too bright (#3).
    const float faceR  = radius - thick * 1.6f;
    const float lift   = 0.06f * hi + 0.06f * aA;
    juce::ColourGradient face (colours::bgRaised.brighter (0.16f + lift), centre.x, centre.y - faceR * 0.7f,
                               colours::bgPanel.darker (0.25f), centre.x, centre.y + faceR, true);
    face.addColour (0.55, colours::bgRaised.brighter (lift));
    g.setGradientFill (face);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
    // Subtle glass rim: bright top-left arc + faint opposite glow (#16).
    glass::drawCircleEdge (g, centre.x, centre.y, faceR, 0.85f + 0.15f * hi);
    g.setColour (colours::outline.brighter (0.12f * hi));
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // Pointer: the glow is a REAL feathered halo AROUND the pointer (a blurred
    // drop-shadow of the pointer shape), not a thick white stroke band on top of it
    // (#2/#8). The solid pointer sits on the glow.
    juce::Path pointer;
    const float pl = faceR * 0.92f, pr = thick * 0.35f;
    pointer.addRoundedRectangle (-pr, -pl, pr * 2.0f, pl * 0.6f, pr);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    // A WIDE, soft feather (a big-radius blurred halo) rather than a tight bright
    // band -- so it reads like the blue/teal arc glow, not a hard white outline
    // (#5). Two radii: a broad soft wash + a closer one. The eased levels fade it
    // in/out and widen it smoothly (F3).
    const float pa = (0.22f + 0.20f * aA) * hi; // hover .22 / press .42, faded by hi
    if (pa > 0.02f)
    {
        const int r1 = 9 + juce::roundToInt (4.0f * aA); // hover 9 / press 13
        juce::DropShadow (juce::Colours::white.withAlpha (pa),        r1,     {}).drawForPath (g, pointer);
        juce::DropShadow (juce::Colours::white.withAlpha (pa * 0.7f), r1 / 2, {}).drawForPath (g, pointer);
    }
    g.setColour (colours::text.brighter (0.2f * hi).interpolatedWith (juce::Colours::white, aA));
    g.fillPath (pointer);
}

void AnabasisLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horizontal = (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // Hover and press are DISTINCT eased levels, like the knob arc: hover glows,
    // press glows MORE (#5). hi = "lit at all", aA = the extra press amount.
    const bool hovB = s.isMouseOver (false);
    const bool interacting = s.isMouseButtonDown()
                          || (bool) s.getProperties().getWithDefault ("dragging", false);
    const float hA = animOr (s, "hovA", hovB);
    const float aA = animOr (s, "actA", interacting);
    const float hi = juce::jmax (hA, aA);

    // Recessed track. The un-filled (dark) portion lifts a touch on hover, more on
    // press -- the same two-level brightening the knob face has (#6).
    const float trackThick = 6.0f;
    const float trackLift = 0.05f * hi + 0.07f * aA;
    juce::Rectangle<float> track = horizontal
        ? juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackThick * 0.5f, bounds.getWidth(), trackThick)
        : juce::Rectangle<float> (bounds.getCentreX() - trackThick * 0.5f, bounds.getY(), trackThick, bounds.getHeight());

    juce::ColourGradient tg (colours::bg.darker (0.25f).brighter (trackLift), track.getX(), track.getY(),
                             colours::bgRaised.brighter (trackLift), track.getX(), track.getBottom(), false);
    g.setGradientFill (tg);
    g.fillRoundedRectangle (track, trackThick * 0.5f);
    g.setColour (colours::outline.brighter (0.10f * hi));
    g.drawRoundedRectangle (track.reduced (0.5f), trackThick * 0.5f, 1.0f);

    // Eased visual position (#7): a preset / A-B switch sweeps the fill + thumb
    // instead of teleporting. During a hand drag we keep the real `pos` so the
    // thumb tracks the cursor exactly 1:1 (the inset lives in getSliderLayout).
    // The `resetSweep` disjunct that used to widen this test went with its rotary
    // twin at round 43 — dead in both paths, and see there for why wiring it
    // would not have worked.
    if (! interacting)
        if (const auto* v = s.getProperties().getVarPointer ("vpos"))
        {
            const float vp = juce::jlimit (0.0f, 1.0f, (float) (double) *v);
            pos = horizontal ? bounds.getX() + vp * bounds.getWidth()
                             : bounds.getBottom() - vp * bounds.getHeight();
        }

    const float r = 8.0f;
    juce::Rectangle<float> fill = horizontal
        ? track.withWidth (juce::jmax (0.0f, pos - bounds.getX()))
        : track.withTop (pos).withBottom (bounds.getBottom());

    // Filled portion: blue->teal gradient with a MANY-layered gradient-opacity
    // halo (NOT a hard widened stroke). The halo is brighter + wider on hover and
    // brighter + wider STILL on press, so the two levels clearly differ (#5).
    const juce::Colour fillLo (0xffe07830), fillHi (0xfff0b432);
    const float glowPeak   = 0.14f + 0.10f * hi + 0.16f * aA; // idle .14 / hover .24 / press .40
    const float glowSpread = 2.6f  + 1.8f  * hi + 2.0f  * aA; // idle 2.6 / hover 4.4 / press 6.4
    constexpr int nLayers = 9;
    for (int i = 0; i < nLayers; ++i)
    {
        const float t  = (float) i / (float) (nLayers - 1); // 0 outer .. 1 inner
        const float ex = (1.0f - t) * glowSpread;
        const float a  = glowPeak * std::pow (t, 1.5f);     // smooth brighten inward
        juce::ColourGradient gg (fillLo.withAlpha (a), fill.getX(), fill.getY(),
                                 fillHi.withAlpha (a),
                                 horizontal ? fill.getRight() : fill.getX(),
                                 horizontal ? fill.getY() : fill.getBottom(), false);
        g.setGradientFill (gg);
        g.fillRoundedRectangle (fill.expanded (ex), (trackThick + ex * 2.0f) * 0.5f);
    }
    juce::ColourGradient fg (fillLo, fill.getX(), fill.getY(),
                             fillHi, horizontal ? fill.getRight() : fill.getX(),
                             horizontal ? fill.getY() : fill.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (fill, trackThick * 0.5f);

    // Glassy thumb: a neutral gray-white rim at rest; on hover a feathered cyan
    // halo, on press a stronger one -- a blurred drop-shadow (gradient opacity),
    // never a hard ring (#5). Two clearly-different levels.
    const float cx = horizontal ? pos : bounds.getCentreX();
    const float cy = horizontal ? bounds.getCentreY() : pos;
    juce::Path thumbPath; thumbPath.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    const float thumbGlow = 0.30f * hi + 0.35f * aA; // hover .30 / press .65
    if (thumbGlow > 0.02f)
    {
        juce::DropShadow (fillHi.withAlpha (thumbGlow),        9, {}).drawForPath (g, thumbPath);
        juce::DropShadow (fillHi.withAlpha (thumbGlow * 0.6f), 4, {}).drawForPath (g, thumbPath);
    }
    // Thumb body fill brightens in TWO distinct levels like the knob face: a
    // little on hover, more on press (#2).
    juce::ColourGradient kg (colours::bgRaised.brighter (0.30f + 0.10f * hi + 0.14f * aA), cx, cy - r,
                             colours::bgPanel.darker (0.18f),     cx, cy + r, false);
    g.setGradientFill (kg);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xffb8c2cf).interpolatedWith (fillHi, hi)); // gray rim -> cyan on touch
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f);
    // Glass rim micro-glow on the thumb ring, always faintly present and a touch
    // brighter on interaction (#5).
    glass::drawCircleEdge (g, cx, cy, r, 1.2f + 0.5f * hi);
}

juce::Slider::SliderLayout AnabasisLookAndFeel::getSliderLayout (juce::Slider& s)
{
    auto layout = juce::LookAndFeel_V4::getSliderLayout (s);
    // Inset the interactive track by the thumb radius: the thumb then maps 1:1 to
    // the cursor within the track and clamps a radius in from each end, so it never
    // hangs off the edge (#4) and never lags the cursor (#5).
    const int rad = 8;
    if (s.isHorizontal())    layout.sliderBounds = layout.sliderBounds.reduced (rad, 0);
    else if (s.isVertical()) layout.sliderBounds = layout.sliderBounds.reduced (0, rad);
    return layout;
}

void AnabasisLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                            bool highlighted, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    // Hover brightens only the switch + text, never the whole block (#4). Both
    // levels are EASED by the micro-anim driver, so the knob slides and the
    // glow fades instead of stepping (F3).
    const float hovA = animOr (b, "hovA", highlighted);
    const float onAv = animOr (b, "onA",  on);
    const float hi = 0.18f * hovA;

    // Two ID-keyed variants used to sit here — `"metersicon"` (a level-meter
    // glyph drawn instead of the word "Meters") and `"vtoggle"` (a compact
    // pill-above-label toggle for tight module rows). Both are removed at
    // review round 54 as unported migration state, on the evidence of who owns
    // them in the sibling product: `metersicon` styles its show/hide LEVEL
    // METERS toggle, and `vtoggle` its Mono / Swap / M-S / polarity-L /
    // polarity-R row. Anabasis has neither — the §6.3 metering strip is always
    // present and has no show/hide control, and the stereo-field toggles belong
    // to a product with a Widen stage. Nothing here ever set either id, so both
    // branches were unreachable, and an unreachable drawing path implies a UI
    // variant a reader may then try to "just use" for a new control.

    // Leave a uniform inset so the outer glow can never be clipped at the left /
    // top / bottom edges (feedback #16). The pill is a fixed, compact size.
    const float pad = 3.0f;
    const float h   = juce::jlimit (12.0f, 18.0f, bounds.getHeight() - pad * 2.0f);
    const float pw  = h * 1.8f;
    auto pill = juce::Rectangle<float> (bounds.getX() + pad,
                                        bounds.getCentreY() - h * 0.5f, pw, h);

    // Bypass uses a controlled red when engaged so it reads as "off/abnormal".
    const juce::Colour onCol = ((b.getComponentID() == "bypass") ? juce::Colour (0xffd0584e)
                                                                  : colours::accent).brighter (hi);
    if (onAv > 0.02f) // soft outer glow (fits inside the pad)
    {
        g.setColour (onCol.withAlpha (0.22f * onAv));
        g.fillRoundedRectangle (pill.expanded (2.0f), (h + 4.0f) * 0.5f);
    }
    const auto pillBase = colours::bgRaised.brighter (hi).interpolatedWith (onCol, onAv);
    juce::ColourGradient pg (pillBase.brighter (0.06f + 0.04f * onAv), pill.getX(), pill.getY(),
                             pillBase.darker (0.10f + 0.02f * onAv),   pill.getX(), pill.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (colours::outline.brighter (hi).interpolatedWith (onCol, onAv));
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);

    const float knob = h - 4.0f;
    const float kx = pill.getX() + 2.0f + (pill.getWidth() - knob - 4.0f) * onAv; // slides (F3)
    g.setColour (colours::text.interpolatedWith (colours::bg, onAv));
    g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

    // Label: fit-to-width so nothing is ever truncated to an ellipsis (#9).
    const float tx = pill.getRight() + 7.0f;
    const float tw = bounds.getRight() - tx - 1.0f;
    if (tw > 4.0f)
    {
        g.setColour (colours::textDim.interpolatedWith (colours::text, hovA));
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.drawFittedText (b.getButtonText(), (int) tx, (int) bounds.getY(),
                          (int) tw, (int) bounds.getHeight(),
                          juce::Justification::centredLeft, 1, 0.85f);
    }
}

void AnabasisLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    if (b.getComponentID() == "ghost") return; // invisible hit-area (e.g. title)

    auto bounds = b.getLocalBounds().toFloat().reduced (1.0f);
    const float radius = juce::jmin (9.0f, bounds.getHeight() * 0.5f); // rounder (#13)
    const float hovA = animOr (b, "hovA", highlighted); // eased hover wash (F3)

    // Preset bar (F2): the name + nav chevrons sit flat on the top bar and only
    // get a quiet rounded wash on hover/press -- FabFilter-style.
    if (b.getComponentID() == "presetname" || b.getComponentID() == "presetnav")
    {
        const float a = 0.55f * hovA + (down ? 0.25f : 0.0f);
        if (a > 0.02f)
        {
            g.setColour (colours::bgRaised.brighter (0.18f).withAlpha (a));
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (colours::outline.withAlpha (a * 0.8f));
            g.drawRoundedRectangle (bounds, radius, 1.0f);
        }
        return;
    }

    const bool on = b.getToggleState();
    const auto base = down ? colours::bgRaised.brighter (0.12f)
                           : colours::bgRaised.brighter (0.06f * hovA);
    if (on)
    {
        g.setColour (colours::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds, radius);
    }
    else
    {
        juce::ColourGradient gr (base.brighter (0.05f), bounds.getX(), bounds.getY(),
                                 base.darker (0.10f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (bounds, radius);
    }
    g.setColour (on ? colours::accent : colours::outline);
    g.drawRoundedRectangle (bounds, radius, 1.0f);
}

juce::Font AnabasisLookAndFeel::getTextButtonFont (juce::TextButton& b, int buttonHeight)
{
    // An `"apply"` row used to precede this one, sizing the sibling product's
    // "Apply Gain" button beside its auto-gain-match readout. Anabasis has no
    // auto-gain match and no Apply button of any kind, so nothing could ever
    // carry the id — removed at review round 54 with the two toggle variants
    // above, and for the same reason.
    if (b.getComponentID() == "icon")       return juce::Font (juce::FontOptions (21.0f)); // bigger glyph (#7)
    if (b.getComponentID() == "presetname") return juce::Font (juce::FontOptions (13.0f)); // preset display (F2)
    if (b.getComponentID() == "presetnav")  return juce::Font (juce::FontOptions (19.0f)); // chevrons (F2)
    return juce::Font (juce::FontOptions ((float) juce::jmin (13, juce::jmax (10, buttonHeight - 12))));
}

juce::Font AnabasisLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return juce::Font (juce::FontOptions (13.5f)); }
juce::Font AnabasisLookAndFeel::getPopupMenuFont()                 { return juce::Font (juce::FontOptions (13.5f)); }

// Position the combo pop-up BELOW the box. The JUCE default adds withItemThatMustBeVisible +
// withInitiallySelectedItem, which centre the popup on the selected row so it COVERS the box (the
// native-macOS look). Targeting the box's screen bounds -- and omitting those two options -- makes
// the menu open flush below the box (or above if there's no room), restoring the drop-down.
juce::PopupMenu::Options AnabasisLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box,
                                                                             juce::Label& label)
{
    return juce::PopupMenu::Options()
             .withTargetComponent (&box)
             .withTargetScreenArea (box.getScreenBounds())
             .withMinimumWidth (box.getWidth())
             .withMaximumNumColumns (1)
             .withStandardItemHeight (label.getHeight());
}

// Pop-up row geometry, in ONE place because two overrides have to agree about it:
// `getIdealPopupMenuItemSize` decides how wide JUCE makes the menu, and
// `drawPopupMenuItem` decides how much of that width the text actually gets. When
// the measurement allowed less than the drawing spends, JUCE sized the menu to the
// smaller number and then clipped the longest row with an ellipsis. Both read these
// constants now, so the two cannot drift apart again.
// The constants themselves now live in `LookAndFeel.h`, because a THIRD reader
// needs them: `state_tests.cpp` reconstructs this row's rectangles to assert
// that a row carrying a shortcut does not draw its label underneath one. A test
// that hard-coded 12 and 14 would pass while agreeing with nothing.
//
// WHAT THIS BUDGET DOES NOT COVER, and how far that reaches. The shortcut IS
// measured — JUCE appends it to the string it hands this function, via
// `ItemComponent::getTextForMeasurement` — and `drawPopupMenuItem` reserves the
// strip it occupies out of the same rectangle, so measured total and drawn total
// describe the same row.
//
// The SUB-MENU CHEVRON is not, and cannot be: this signature carries no
// `hasSubMenu`, so a budget computed here has no way to know. `drawPopupMenuItem`
// reserves the chevron's gap anyway, out of the label, so the two can never
// collide — a sub-menu row degrades to an ellipsised label (~7 px at this row
// height) rather than to overlapping glyphs.
//
// WHO CAN ACTUALLY REACH THAT, checked against the pinned JUCE rather than
// assumed, because the look-and-feel serves menus this editor does not build:
//   * `TextEditor::addPopupMenuItems` (juce_TextEditor.cpp) builds Cut/Copy/
//     Paste/Delete/Select All/Undo/Redo — FLAT, no `addSubMenu`, on every
//     platform. The cross-platform implementation is the whole of it; there is
//     no OS services sub-menu grafted on.
//   * `ComboBox::showPopup` adds items from its own flat list.
//   * `Slider::showPopupMenu` DOES `addSubMenu ("Rotary mode", …)` — the one
//     sub-menu in the widgets we use. It is gated on `setPopupMenuEnabled`,
//     which this editor never calls and which JUCE leaves off, so no slider here
//     can open it.
// So the case is unreachable today by construction rather than by luck, and if a
// slider ever enables that menu the cost is a 7 px shorter label on one row.
// Widening `chrome` for every menu in the plug-in to pre-pay for it is the worse
// trade, and `testEveryComboMenuFitsItsControl` is what would notice if it were
// made.

void AnabasisLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                     int, int& idealWidth, int& idealHeight)
{
    if (isSeparator) { idealWidth = 60; idealHeight = 8; return; }
    auto f = getPopupMenuFont();
    juce::GlyphArrangement ga;
    // JUCE hands us the shortcut already appended to the text for measurement
    // (`ItemComponent::getTextForMeasurement`), so measuring the argument covers
    // both halves of what the row draws.
    ga.addLineOfText (f, text, 0.0f, 0.0f);
    const auto textW = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth());
    idealWidth  = juce::jmax (menuMetrics::minimumRow, textW + (int) menuMetrics::chrome);
    idealHeight = 23; // uniform across every combo regardless of its on-screen height (#3)
}

// Undo/Redo glyphs: render them larger AND rotated 180 degrees, which the user
// found more comfortable (feedback #7). All other buttons use the default text.
void AnabasisLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                          bool /*highlighted*/, bool /*down*/)
{
    if (b.getComponentID() == "icon")
    {
        auto area = b.getLocalBounds().toFloat();
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (colours::text.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi,
                                                         area.getCentreX(), area.getCentreY()));
        // Nudge the (rotated) glyph so it reads as optically centred (#8): shifting
        // the pre-rotation box UP moves the visible glyph DOWN.
        g.drawText (b.getButtonText(), area.translated (0.0f, -2.0f), juce::Justification::centred, false);
        return;
    }
    if (b.getComponentID() == "presetname") // preset display (F2)
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (colours::text);
        g.drawText (b.getButtonText(), b.getLocalBounds().reduced (6, 0),
                    juce::Justification::centred, true);
        return;
    }
    if (b.getComponentID() == "presetnav") // ‹ › chevrons brighten on hover (F2/F3)
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (colours::textDim.interpolatedWith (colours::text, animOr (b, "hovA", b.isOver())));
        g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, -1),
                    juce::Justification::centred, false);
        return;
    }
    juce::LookAndFeel_V4::drawButtonText (g, b, false, false);
}

// Pop-up rows: a clean, FLAT modern list -- the highlighted row is a single
// solid accent tint with no gradient, sheen or bevel (the previous glassy version
// read as dated "Vista aero", #3).
void AnabasisLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool isSeparator, bool isActive, bool isHighlighted,
                                             bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String& shortcutKeyText,
                                             const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        auto r = area.toFloat().reduced (8.0f, 0.0f);
        g.setColour (colours::outline.withAlpha (0.7f));
        g.fillRect (r.withHeight (1.0f).withY (r.getCentreY()));
        return;
    }

    auto r = area.toFloat();
    // A row JUCE reports as inactive cannot be chosen, and until now it drew
    // exactly like one that can: same text, same tick, same arrow. The whole row
    // is dimmed instead, so "unavailable" reads before the click rather than after
    // it does nothing.
    const float itemAlpha = ! isActive     ? menuMetrics::inactive
                          : isHighlighted  ? 1.0f
                                           : menuMetrics::dim;
    const auto  labelCol  = colours::text.withMultipliedAlpha (itemAlpha);

    if (isHighlighted && isActive)
    {
        // One flat accent tint, lightly rounded -- clean and modern (#3).
        g.setColour (colours::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (r.reduced (3.0f, 1.0f), 4.0f);
    }

    g.setColour (labelCol);
    g.setFont (getPopupMenuFont());

    auto textArea = r.reduced (menuMetrics::padX, 0.0f);
    if (isTicked)
    {
        auto tick = textArea.removeFromLeft (menuMetrics::tickGutter);
        g.setColour (colours::accent.withMultipliedAlpha (itemAlpha));
        g.fillEllipse (tick.getCentreX() - 2.0f, tick.getCentreY() - 2.0f, 4.0f, 4.0f);
        g.setColour (labelCol);
    }
    else
    {
        textArea.removeFromLeft (menuMetrics::tickGutter);
    }

    // The right-hand furniture is taken OUT of the label's rectangle before the
    // label is drawn. Both pieces used to be drawn over the same rectangle the
    // label had already filled, guarded by a pair of `jassert`s declaring that
    // neither could occur — which made a supported JUCE feature a debug-build
    // hard stop, in a look-and-feel that also serves menus this editor does not
    // build (a `TextEditor`'s own context menu inherits it through
    // `getLookAndFeel()`). The row now renders whatever it is handed:
    //
    //   * The SHORTCUT is measured into the row's width by JUCE — the ideal-size
    //     call receives `text + "   " + shortcut` from `getTextForMeasurement`,
    //     so the budget already covers it — and drawn in the strip reserved here.
    //     Reserving it is what turns a pathological row (a long label whose
    //     shortcut the smaller font does not shrink enough) into an ellipsised
    //     label instead of two runs of glyphs on top of each other.
    //   * The CHEVRON is drawn in the right-hand inset, outside the label's
    //     rectangle already; the gap taken here is only so a full-width label
    //     cannot touch it. Nothing measures it, so a sub-menu row spends that
    //     gap out of its own label — bounded, visible, and preferable to the
    //     alternative of widening `menuMetrics::chrome` for every menu in the
    //     plug-in to reserve space almost none of them use.
    const float chevronGap = hasSubMenu ? (float) area.getHeight() * 0.12f + 4.0f : 0.0f;
    textArea.removeFromRight (chevronGap);

    if (shortcutKeyText.isNotEmpty())
    {
        const auto shortcutFont = getPopupMenuFont().withHeight (menuMetrics::shortcutPt);
        juce::GlyphArrangement ga;
        ga.addLineOfText (shortcutFont, shortcutKeyText, 0.0f, 0.0f);
        const auto w = ga.getBoundingBox (0, -1, true).getWidth();
        const auto strip = textArea.removeFromRight (
                               juce::jmin (w + menuMetrics::shortcutGap,
                                           textArea.getWidth() * 0.5f));

        g.setColour (colours::textDim.withMultipliedAlpha (itemAlpha));
        g.setFont (shortcutFont);
        g.drawText (shortcutKeyText, strip, juce::Justification::centredRight);
        g.setColour (labelCol);
        g.setFont (getPopupMenuFont());
    }

    g.drawText (text, textArea, juce::Justification::centredLeft);

    if (hasSubMenu)
    {
        const float h = (float) area.getHeight();
        juce::Path p;
        const float x = r.getRight() - menuMetrics::padX, cy = r.getCentreY();
        p.startNewSubPath (x, cy - h * 0.12f);
        p.lineTo (x + h * 0.12f, cy);
        p.lineTo (x, cy + h * 0.12f);
        g.setColour (colours::textDim.withMultipliedAlpha (itemAlpha));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
}

// FACTORY / USER section headers in the preset menu: small dim caps (F2).
void AnabasisLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                      const juce::String& sectionName)
{
    g.setColour (colours::textDim.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.5f)).withExtraKerningFactor (0.18f));
    // The SAME inset AND tick gutter the item rows spend, so "FACTORY" / "USER"
    // sit on the text column they head instead of 14 px to the left of it. This
    // path was left out when the item metrics were unified: it hard-coded its own
    // 12 and reserved no gutter.
    //
    // ALIGNMENT ONLY — the width side needs nothing and is deliberately not
    // overridden. `LookAndFeel_V2::getIdealPopupMenuSectionHeaderSizeWithOptions`
    // forwards to `getIdealPopupMenuItemSizeWithOptions`, which reaches the
    // override above, and then adds a quarter to the result. So a header is
    // measured with THIS family's item budget in the larger item font and then
    // widened again, while it draws in a 10.5 pt font — over-measured on both
    // counts, which is the safe direction. Overriding it to be exact would only
    // narrow menus, and could do so for a reason no reader would connect to a
    // header.
    auto textArea = area.toFloat().reduced (menuMetrics::padX, 0.0f);
    textArea.removeFromLeft (menuMetrics::tickGutter);
    g.drawText (sectionName, textArea, juce::Justification::centredLeft);
}

void AnabasisLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    // Square, fully-opaque list: rounded corners on an opaque menu window leave
    // bright corner/edge artefacts on some hosts, so we keep the popup square and
    // clean (feedback #3). The flat fill + hairline still reads as premium.
    g.fillAll (colours::bgPanel);
    g.setColour (colours::outline);
    g.drawRect (0, 0, width, height, 1);
}

void AnabasisLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const float radius = juce::jmin (5.0f, bounds.getHeight() * 0.5f); // a touch squarer (#8)
    // Hover / open feedback (#10): brighten the WHOLE body, accent the outline.
    // The editor's timer publishes an authoritative single-combo hover flag (#20),
    // falling back to a live cursor test before the first timer tick.
    const bool hover = box.getProperties().contains ("hov")
                     ? (bool) box.getProperties()["hov"]
                     : box.getLocalBounds().contains (box.getMouseXYRelative());
    const bool open  = down || box.isPopupActive();
    // Hover lift is EASED by the micro-anim driver; the open lift stays instant
    // so the box visibly anchors its list (F3).
    const float lift = 0.05f + 0.07f * animOr (box, "hovA", hover) + (open ? 0.06f : 0.0f);
    juce::ColourGradient gr (colours::bgRaised.brighter (lift), bounds.getX(), bounds.getY(),
                             colours::bgRaised.darker (0.10f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (bounds, radius);
    if (open)
    {
        // Open state: a faint accent bloom just inside the rim plus a thin
        // gradient border -- the box reads as lit by its list, instead of the
        // previous flat thick ring, which looked dated (#2).
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colours::accent.withAlpha (0.05f * (float) i));
            g.drawRoundedRectangle (bounds.reduced ((float) (4 - i)),
                                    juce::jmax (1.5f, radius - (float) (4 - i)), 1.8f);
        }
        juce::ColourGradient og (colours::accent.brighter (0.22f), bounds.getX(), bounds.getY(),
                                 colours::accent.withAlpha (0.55f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (og);
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }
    else
    {
        g.setColour (colours::outline);
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }

    juce::Path arrow;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 4, cy - 2);
    arrow.lineTo (cx, cy + 3);
    arrow.lineTo (cx + 4, cy - 2);
    g.setColour (colours::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

void AnabasisLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    // Start the text a little further from the left edge so it isn't cramped
    // against the border (#13).
    label.setBounds (9, 1, box.getWidth() - 9 - 20, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

// Text fields carrying a "glow" property (the Save-Preset name box) get a
// rounded, faintly accent-lit border when focused -- the same micro-glow as an
// open combo, so it reads premium rather than a thin default rectangle (#11).
void AnabasisLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                    juce::TextEditor& ed)
{
    if (! (bool) ed.getProperties().getWithDefault ("glow", false))
    {
        juce::LookAndFeel_V4::fillTextEditorBackground (g, width, height, ed);
        return;
    }
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (ed.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (b, 5.0f);
}

void AnabasisLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                 juce::TextEditor& ed)
{
    if (! (bool) ed.getProperties().getWithDefault ("glow", false))
    {
        juce::LookAndFeel_V4::drawTextEditorOutline (g, width, height, ed); // value boxes unchanged
        return;
    }

    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float radius = 5.0f;

    if (ed.hasKeyboardFocus (true))
    {
        // Faint accent bloom just inside the rim + a thin vertical-gradient border.
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colours::accent.withAlpha (0.045f * (float) i));
            g.drawRoundedRectangle (b.reduced ((float) (4 - i)),
                                    juce::jmax (1.5f, radius - (float) (4 - i)), 1.8f);
        }
        juce::ColourGradient og (colours::accent.brighter (0.20f), b.getX(), b.getY(),
                                 colours::accent.withAlpha (0.55f), b.getX(), b.getBottom(), false);
        g.setGradientFill (og);
        g.drawRoundedRectangle (b, radius, 1.2f);
    }
    else
    {
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b, radius, 1.0f);
    }
}

juce::Font AnabasisLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

// Default LookAndFeel::drawLabel routes through getLabelFont(), which forced a
// single size and silently overrode every per-label setFont() -- which is why the
// larger Simple-mode Widen text never appeared. Honour the label's own font here.
void AnabasisLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font (label.getFont()); // <- explicit per-label font wins
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);

        auto area = label.getBorderSize().subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());
    }

    g.setColour (label.findColour (juce::Label::outlineColourId));
    g.drawRect (label.getLocalBounds());
}

// A slider value box you can DRAG (up/down) to change the value (#28), while a
// double-click opens an editor pre-filled with the RAW number (no %, dB, Hz),
// which still parses unit/k-suffixed input (#36 / #37 / #29).
namespace
{
    // ONE predicate for all three handlers, which is the point of it. It was a
    // STYLE test before round 44, and only `mouseDown`/`mouseDrag` used it:
    // `mouseUp` cleared the "dragging" property through a plain
    // `dynamic_cast<juce::Slider*>`, a WIDER test than the one that set it. Two
    // predicates for one pairing is how a begin ends up without its end.
    //
    // The style test also scoped the drag BRACKET to rotary parents, so a value
    // box on a linear slider would have written the parameter unbracketed —
    // exactly the defect the bracket was added to remove, waiting for the first
    // linear slider to be added. `setupRotary` builds every ValueBox in the tree
    // (the one `new ValueBox()` below) and builds only rotary sliders, so
    // widening this to "any slider parent" changes NOTHING today; it means the
    // trap cannot be re-armed by adding a control.
    static juce::Slider* sliderParent (juce::Component* c) noexcept
    {
        return dynamic_cast<juce::Slider*> (c);
    }

    static juce::String rawEditText (juce::Slider& s)
    {
        // Mirror the displayed value exactly (what's outside is what you edit),
        // just without the unit (#3/#4/#5). Derive from the display string.
        //
        // A `"unit" == "bal"` branch used to sit here, decoding an "L 25" /
        // "C" / "R 30" display into a signed number. It was unreachable — no
        // slider in this tree ever set a `"unit"` property — and it was also
        // unnecessary: it decodes the sibling product's stereo Balance format,
        // and Anabasis's `colourBalance` registers
        // `[](float v, int) { return juce::String (v, 2); }`, a plain signed
        // decimal that the generic path below already passes through untouched.
        // Removed rather than wired, because wiring it would have taught the
        // editor to parse a display string this product does not produce.
        juce::String disp = s.getTextFromValue (s.getValue()).trim();

        if (disp.endsWithIgnoreCase ("kHz"))                            // "5.55 kHz" -> "5.55k"
            return disp.dropLastCharacters (3).trim() + "k";
        const int sp = disp.lastIndexOfChar (' ');                      // strip a trailing unit word
        if (sp > 0 && ! disp.substring (sp + 1).containsAnyOf ("0123456789"))
            return disp.substring (0, sp).trim();
        return disp;
    }

    struct ValueBox : public juce::Label
    {
        double downProp = 0.0;
        // The drag below writes the parameter, so it is a GESTURE and must be
        // bracketed like every other one. Unbracketed it was neither an undo
        // step (§7 keys on a completed message-thread drag) nor a DETACH (§5.3
        // keys on "gesture-bracketed"), so dragging a managed parameter's
        // numeric readout behaved differently from dragging its knob — the
        // same asymmetry the double-click fix removed one control over.
        // `Slider::ScopedDragNotification` is the stock RAII bracket: it calls
        // `startedDragging`/`stoppedDragging`, which is exactly what the
        // SliderAttachment listens to in order to open and close the host
        // gesture, so this needs no knowledge of the parameter itself.
        std::unique_ptr<juce::Slider::ScopedDragNotification> drag;
        bool downArmed = false;   // did THIS press record `downProp`? see mouseDown

        void mouseDown (const juce::MouseEvent& e) override
        {
            // Close any bracket still open before opening the next. Assigning
            // over a live `unique_ptr` runs the NEW constructor (begin) before
            // the old destructor (end), so a mouse-down that never saw its
            // mouse-up — a re-parent mid-press, a stolen capture, a synthetic
            // event — would hand the host begin/begin/end/end and trip JUCE's
            // `jassert (! isPerformingGesture)`. Resetting first makes the
            // pairing unconditional; the pointer is null in the ordinary case.
            drag.reset();
            downArmed = false;
            if (auto* s = sliderParent (getParentComponent()); s != nullptr && e.getNumberOfClicks() < 2 && ! isBeingEdited())
            {
                // The drag ORIGIN and the press feedback are recorded here; the
                // gesture is NOT opened until the pointer actually moves — see
                // `mouseDrag`.
                //
                // `downArmed` is what makes the recording and the consuming ONE
                // predicate. `mouseDrag` used to re-derive its own, narrower
                // condition (`sliderParent && ! isBeingEdited()`), which is not
                // the condition under which `downProp` was written: on a text
                // box that is NOT editable a double-click opens no editor, so
                // `isBeingEdited()` stays false and the drag would have run
                // against a `downProp` captured at the FIRST click while
                // `getDistanceFromDragStartY()` measured from the second press.
                // Inert today — `createSliderTextBox` makes every box editable
                // when its slider is (`setEditable (false, s.isTextBoxEditable(),
                // false)`), and `setupRotary` builds them all editable, so a
                // double-click always opens the editor and the drag falls
                // through to `juce::Label` — but "one path records under a
                // predicate another path does not check" is the exact asymmetry
                // this file spent several rounds removing.
                downArmed = true;
                downProp = s->valueToProportionOfLength (s->getValue());
                s->getProperties().set ("dragging", true); // knob shows press feedback (#10)
                s->repaint();
            }
            juce::Label::mouseDown (e); // double-click still opens the editor
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            // Closed here rather than in `mouseDrag`'s branch: a press with no
            // movement must still balance the begin, and an aborted gesture
            // that changed nothing pushes no undo step anyway (§7 compares the
            // pre-state). Reset unconditionally — the pointer is null unless
            // `mouseDown` opened one.
            drag.reset();
            downArmed = false;
            if (auto* s = sliderParent (getParentComponent()))
            {
                s->getProperties().set ("dragging", false);
                s->repaint();
            }
            juce::Label::mouseUp (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            // Directly map vertical drag to the value (respecting any skew). This
            // is robust regardless of event routing -- the previous forwarding
            // approach didn't take (feedback #28).
            if (auto* s = sliderParent (getParentComponent()); s != nullptr && downArmed && ! isBeingEdited())
            {
                // THE GESTURE OPENS HERE, on the first movement, not on
                // mouse-down. It was opened in `mouseDown`, which made a plain
                // CLICK on the number a complete host gesture — and on the three
                // §5.5 macros a gesture-begin is not a neutral event: it takes
                // the macro branch of `audioProcessorParameterChangeGestureBegin`,
                // which clears the WHOLE detach mask and re-lands the curve. So
                // clicking the number under Loudness to read it, or as the first
                // half of a double-click to type into it, discarded every manual
                // Advanced edit the user had made. §5.3 makes a macro gesture
                // "the clear notice" that the user is choosing the macro over
                // their edits; pressing on a numeric readout is not that notice.
                //
                // Moving it here draws the line the code already drew one event
                // later: `mouseDown` refuses the bracket on a double-click for
                // exactly the "about to type" reason, and a press that never
                // moves is the same case. What is unchanged: a real drag opens
                // the bracket BEFORE its first write, so every parameter write
                // still happens inside it — the imbalance the bracket was added
                // to remove cannot come back — and the knob's own behaviour is
                // untouched, since `juce::Slider` opens its gesture on press and
                // that is a genuine macro grab.
                if (drag == nullptr)
                    drag = std::make_unique<juce::Slider::ScopedDragNotification> (*s);
                const double prop = juce::jlimit (0.0, 1.0, downProp + (-e.getDistanceFromDragStartY()) / 180.0);
                s->setValue (s->proportionOfLengthToValue (prop), juce::sendNotificationSync);
            }
            else
                juce::Label::mouseDrag (e);
        }
        void editorShown (juce::TextEditor* ed) override
        {
            if (auto* s = dynamic_cast<juce::Slider*> (getParentComponent()))
            {
                ed->setText (rawEditText (*s), false); // raw number only (#36)
                ed->selectAll();
            }
        }
    };
}

juce::Label* AnabasisLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = new ValueBox();
    l->setJustificationType (juce::Justification::centred);
    l->setFont (juce::Font (juce::FontOptions (13.0f))); // explicit default; Simple mode enlarges it (#A)
    l->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
    l->setColour (juce::Label::textColourId,            s.findColour (juce::Slider::textBoxTextColourId));
    l->setColour (juce::Label::backgroundColourId,      juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId,         juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundWhenEditingColourId, colours::bg);
    l->setColour (juce::Label::textWhenEditingColourId, colours::text);
    l->setColour (juce::TextEditor::highlightColourId,  colours::accent.withAlpha (0.30f));
    l->setColour (juce::TextEditor::textColourId,       colours::text);
    l->setEditable (false, s.isTextBoxEditable(), false); // double-click to type
    return l;
}

// ---- Tooltips: rounded dark capsule, accent hairline, soft text (#20) ----
static juce::TextLayout layoutTooltip (const juce::String& text, float maxWidth)
{
    juce::AttributedString s;
    s.append (text, juce::Font (juce::FontOptions (12.5f)), colours::text);
    s.setJustification (juce::Justification::centredLeft);
    juce::TextLayout tl;
    tl.createLayout (s, maxWidth);
    return tl;
}

juce::Rectangle<int> AnabasisLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos,
                                                            juce::Rectangle<int> parentArea)
{
    auto tl = layoutTooltip (tip, 260.0f);
    const int w = (int) std::ceil (tl.getWidth())  + 20;
    const int h = (int) std::ceil (tl.getHeight()) + 14;
    return juce::Rectangle<int> (pos.x > parentArea.getCentreX() ? pos.x - (w + 12) : pos.x + 14,
                                 pos.y > parentArea.getCentreY() ? pos.y - (h + 8)  : pos.y + 16,
                                 w, h).constrainedWithin (parentArea);
}

void AnabasisLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto full = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    auto b = full.reduced (1.0f);
    // On platforms WITHOUT per-pixel window alpha (notably Linux/X11 with no compositor),
    // juce::TooltipWindow cannot be semi-transparent, so the area OUTSIDE the rounded capsule
    // renders the window's opaque fill -> black corners (ANAMORPH's KI-006, whose numbering this
    // adapted file inherited; Anabasis has no entry for it -- and its own KI-006 is an unrelated
    // Freeze/re-prepare issue, so the bare reference pointed a reader at the wrong document).
    // Pre-fill the whole bounds with
    // the capsule colour so the corners match the capsule instead of rendering black. Where
    // transparent windows ARE available (macOS / Windows / compositing Linux) the corners stay
    // genuinely transparent -- no visual change there. NOTE: juce::TooltipWindow declares itself
    // opaque by default, which makes these unpainted corners UNDEFINED pixels; on macOS the
    // editor marks its TooltipWindow non-opaque so the peer clears them to real alpha every
    // paint (the Apple-Silicon-native white-corner fix, 0.8.10). That marking was NOT carried
    // across with this file and was owed the moment the editor handed this LookAndFeel to its
    // TooltipWindow at all -- a parentless desktop tooltip inherits nothing, so until then it
    // resolved the DEFAULT look-and-feel and none of this ran. Both halves now exist:
    // `PluginEditor.cpp` sets the look-and-feel and, under JUCE_MAC, `setOpaque (false)`.
    if (! juce::Desktop::getInstance().canUseSemiTransparentWindows())
    {
        g.setColour (colours::bgRaised);
        g.fillRect (full);
    }
    // Subtle glass capsule -- the previous version was too white / contrasty (#7).
    glass::fillPanel (g, b, 6.0f, colours::bgRaised, 0.7f);
    g.setColour (colours::accent.withAlpha (0.28f)); // faint accent hairline
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
    layoutTooltip (text, (float) w - 20.0f).draw (g, b.reduced (10.0f, 7.0f));
}

// ---- Glass surfaces (#7/#17) -----------------------------------------------
namespace glass
{
    void drawEdges (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float strength)
    {
        // A restrained, 0.5.4-style micro-glow: a dim hairline plus a soft top-left
        // highlight that fades gently toward the centre, and a fainter bottom-right
        // one. Deliberately NOT a bright, thick rim -- that read as grey and stiff
        // (#1). A hint of corner detail remains.
        auto r = bounds.reduced (0.5f);
        const auto c = r.getCentre();

        g.setColour (colours::outline);
        g.drawRoundedRectangle (r, radius, 1.0f);

        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.20f * strength), r.getX(), r.getY(),
                                     juce::Colours::white.withAlpha (0.0f), c.x, c.y, false);
            g.setGradientFill (gr);
            g.drawRoundedRectangle (r, radius, 1.3f);
        }
        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.09f * strength), r.getRight(), r.getBottom(),
                                     juce::Colours::white.withAlpha (0.0f), c.x, c.y, false);
            g.setGradientFill (gr);
            g.drawRoundedRectangle (r, radius, 1.1f);
        }
    }

    void fillPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius,
                    juce::Colour base, float strength)
    {
        // Gentle VERTICAL depth (0.5.3 feel): a little brighter at the top, darker
        // toward the bottom -- no bright grey diagonal wash (#1/#7).
        juce::ColourGradient gr (base.brighter (0.04f * strength), bounds.getCentreX(), bounds.getY(),
                                 base.darker  (0.20f * strength), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (bounds, radius);
        drawEdges (g, bounds, radius, strength);
    }

    void drawCircleEdge (juce::Graphics& g, float cx, float cy, float radius, float strength)
    {
        auto box = juce::Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f).reduced (0.4f);
        // One corner (top-left) catches a clear glass highlight; the opposite edge
        // gets a faint glow -- subtle, like the panels (#16).
        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.42f * strength), box.getX(), box.getY(),
                                     juce::Colours::white.withAlpha (0.0f), cx, cy, false);
            g.setGradientFill (gr);
            g.drawEllipse (box, 1.4f);
        }
        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.16f * strength), box.getRight(), box.getBottom(),
                                     juce::Colours::white.withAlpha (0.0f), cx, cy, false);
            g.setGradientFill (gr);
            g.drawEllipse (box, 1.1f);
        }
    }
} // namespace glass

} // namespace abgui
