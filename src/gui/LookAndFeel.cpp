#include "LookAndFeel.h"

AnabasisLookAndFeel::AnabasisLookAndFeel()
{
    // Placeholder ground colour only — the real palette is a P5 brand
    // decision (DESIGN §6.1) and is not invented here (C8).
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff141618));
}
