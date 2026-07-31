#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

// ============================================================================
//  MacroEngine — the Simple-mode macro→managed-parameter mapper (ADR-0005,
//  DESIGN §5.2/§5.5).
//
//  MESSAGE-THREAD-ONLY BY CONSTRUCTION (ADR-0011 / THREADING_POLICY): the
//  macros are non-automatable and this engine consumes their changes solely
//  through an async listener, so a host that automates one anyway gets the
//  mapping at message-thread rate and offline-render determinism is explicitly
//  not promised for that unsupported usage.
//
//  P1 scope: the §5.5 draft curves and the direct apply, with the re-entrancy
//  flag that keeps macro-originated writes from being taken for user edits.
//  The §5.3 detach/re-engage latch and gesture bracketing land at P4; the
//  detach-mask STORAGE already exists in the schema (ADR-0007) so P4 changes
//  no contract.
//
//  Binding rule (MODE_AND_ADAPTATION_POLICY invariant 1, guarded by
//  testMacroDefaultIsFixedPoint): M(0,0,0) must equal every managed
//  parameter's declared §4.2 default. The curves are pure functions so the
//  test exercises exactly what the engine applies.
// ============================================================================

namespace macro_curves
{
    // DESIGN §5.5 (⊕ draft — tuned by ear at P4, frozen before v0.1.0).
    // l = loudness/100 in 0..1, character in 0..1, tone in -1..+1.
    inline float limGainDb      (float l)          { return 18.0f * std::pow (l, 1.2f); }
    inline float compThresholdDb(float l)          { return -12.0f * std::min (1.0f, l / 0.6f); }
    inline float compRatio      (float l)          { return 1.5f + 0.5f * l; }
    inline float clipDriveDb    (float l)          { return l < 0.3f ? 0.0f : 9.0f * (l - 0.3f) / 0.7f; }
    inline float clipShape      (float l)          { return l < 0.3f ? 0.5f
                                                         : 0.5f - 0.15f * (l - 0.3f) / 0.7f; } // 0.5→0.35 over l=0.3…1
    inline float colourDepthPct (float l, float c) { return 100.0f * c * (0.4f + 0.6f * l); }
    inline float dynTiltDb      (float l)          { return l < 0.5f ? 0.0f : 1.5f * (l - 0.5f) / 0.5f; } // 0→1.5 over l=0.5…1
    inline float eqTiltDb       (float t)          { return t * 2.0f; }
    inline float colourTone     (float t)          { return t * 0.5f; }
}

class MacroEngine : private juce::AudioProcessorValueTreeState::Listener,
                    private juce::AsyncUpdater
{
public:
    explicit MacroEngine (juce::AudioProcessorValueTreeState& apvtsIn);
    ~MacroEngine() override;

    // True while the engine itself is writing managed parameters — the §5.3
    // discriminator's "not macro-originated" half. P4's gesture bracketing is
    // the other half.
    bool isApplyingMacro() const noexcept { return applying; }

private:
    void parameterChanged (const juce::String&, float) override;   // any thread → async hop
    void handleAsyncUpdate() override;                             // message thread only
    void applyMapping();
    void setParam (const char* paramID, float denormalisedValue);

    juce::AudioProcessorValueTreeState& apvts;
    bool applying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroEngine)
};
