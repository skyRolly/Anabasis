#include "PluginEditor.h"
#include "../PluginProcessor.h"

AnabasisAudioProcessorEditor::AnabasisAudioProcessorEditor (AnabasisAudioProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (940, 720);            // §6.2 Simple-view frame (⊕ ratified)
#if JUCE_MAC || JUCE_WINDOWS
    glContext.attachTo (*this);    // §6.1: GPU compositing on these two only
#endif
}

AnabasisAudioProcessorEditor::~AnabasisAudioProcessorEditor()
{
#if JUCE_MAC || JUCE_WINDOWS
    glContext.detach();
#endif
    setLookAndFeel (nullptr);
}

void AnabasisAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (20.0f));
    g.drawText ("ANABASIS " ANABASIS_VERSION_STRING " — P1 skeleton",
                getLocalBounds(), juce::Justification::centred);

    // Reads the processor deliberately: the reported latency is the one P1
    // fact worth showing, and an unread private reference trips clang's
    // -Wunused-private-field (inside -Wall, so the macOS/Windows legs would
    // break CODE_STYLE's warning-free rule while GCC stayed quiet).
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("reported latency: " + juce::String (processor.getLatencySamples()) + " samples",
                getLocalBounds().reduced (12), juce::Justification::centredBottom);
}
