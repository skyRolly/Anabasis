#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#if JUCE_MAC || JUCE_WINDOWS
 #include <juce_opengl/juce_opengl.h>
#endif
#include "LookAndFeel.h"

class AnabasisAudioProcessor;

// ============================================================================
//  AnabasisAudioProcessorEditor — P1 skeleton editor.
//
//  Geometry: 940×720 logical, the DESIGN §6.2 Simple-view frame (⊕ ratified
//  at sign-off; Anamorph's frame sizes). Views, top bar and backdrops land at
//  P5 per §6.2/§6.3.
//
//  OpenGL: attached on macOS/Windows, NEVER on Linux/X11 (DESIGN §6.1,
//  ADR-0011 §6.1 note). The module is linked on every platform; only the
//  attach is gated — which is why this include compiles everywhere.
// ============================================================================

class AnabasisAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AnabasisAudioProcessorEditor (AnabasisAudioProcessor&);
    ~AnabasisAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    AnabasisAudioProcessor& processor;
    AnabasisLookAndFeel lookAndFeel;
#if JUCE_MAC || JUCE_WINDOWS
    juce::OpenGLContext glContext;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnabasisAudioProcessorEditor)
};
