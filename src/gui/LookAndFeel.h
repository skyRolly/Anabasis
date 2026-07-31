#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
//  AnabasisLookAndFeel — P1 stub. The brand system (family palette, glass
//  language, control drawing) is copied-and-adapted from Anamorph at P5
//  (ADR-0009, DESIGN §6.1); until then this only pins the dark ground so the
//  skeleton editor is not unstyled white.
// ============================================================================

class AnabasisLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnabasisLookAndFeel();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisLookAndFeel)
};
