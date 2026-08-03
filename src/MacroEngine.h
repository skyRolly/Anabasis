#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>

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

// The §5.5 managed set — ONE list, shared by the mapper (which writes these)
// and the wrapper's §5.3 detach discriminator (which watches them), so the
// two cannot drift apart. clipMix/compMix are deliberately NOT managed.
namespace managed_params
{
    inline constexpr const char* ids[] = {
        "limGain", "compThreshold", "compRatio", "clipDrive", "clipShape",
        "colourDepth", "dynTilt", "eqTilt", "colourTone",
    };
    inline constexpr int kCount = 9;
}

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
                    private juce::AsyncUpdater,
                    private juce::Timer
{
public:
    explicit MacroEngine (juce::AudioProcessorValueTreeState& apvtsIn);
    ~MacroEngine() override;

    // True while the engine itself is writing managed parameters — the §5.3
    // discriminator's "not macro-originated" half. P4's gesture bracketing is
    // the other half.
    bool isApplyingMacro() const noexcept
    { return applying.load (std::memory_order_relaxed); }

    // §5.3 detach filter: the wrapper owns the mask (it is per-A/B-slot,
    // serialized state); the mapper only ASKS. Null = nothing detached.
    std::function<bool (const char*)> isDetached;

    // The wrapper's own message-thread drain, run on THIS object's 30 ms timer
    // tick. It exists so the wrapper never has to post its own message from a
    // listener callback: APVTS and gesture callbacks arrive on whichever thread
    // the host chooses, `triggerAsyncUpdate()` takes a lock (and allocates on
    // some platforms), and that is a REALTIME_AUDIO_POLICY hard red line if the
    // thread turns out to be the audio one. This class already refused to post
    // for exactly that reason and already runs the timer; sharing the tick
    // keeps one cadence instead of two. Called on the message thread only.
    std::function<void()> onDrainTick;

    // Exactly what the 30 ms timer does, exposed so the ORDER inside it is
    // testable: the two halves are ordered against each other (the wrapper's
    // bits decide the mask the mapping reads), and an order that only exists
    // inside a private timer callback is an order no test can pin.
    void drainTick();

    // Starts the 30 ms drain. Call ONCE, after both callbacks above are
    // assigned: they are read on the tick, and the owner cannot assign them
    // until this object is constructed. Until it is called nothing drains, so
    // a mapping armed in between simply waits — the flag is never lost.
    void startDraining();

    // The symmetric half, and it is owed for the symmetric reason: the tick
    // calls back INTO the owner, whose members are destroyed before this
    // object is (it is declared early, and members die in reverse declaration
    // order). The owner calls this FIRST in its destructor, so no tick can
    // reach a half-destroyed owner. Splitting the construction race out and
    // leaving the destruction one would have been half a fix.
    void stopDraining();

    // True while any ScopedRestore is alive — the detach discriminator's
    // third condition (a restore lands values, it is not a user gesture).
    bool isRestoring() const noexcept
    { return restoreDepth.load (std::memory_order_relaxed) > 0; }

    // §5.3 step 4, "reset to macro": re-run the mapping at the CURRENT macro
    // position (message thread). The caller clears the mask first; this is
    // what re-lands the curve values on the freshly re-engaged parameters
    // without the macro itself moving.
    // NOTE it goes through `flushPendingMapping` → `drainTick`, so it also
    // runs the WRAPPER's detach drain synchronously — see the re-entrancy
    // paragraph in `drainTick`. Callers must have replaced the detach mask
    // (which clears the staged bits) before reaching here, which every one of
    // them does; the alternative would be a mask write landing in the middle
    // of a preset apply.
    void refreshMapping()
    {
        mappingPending.store (true, std::memory_order_relaxed);
        flushPendingMapping();
    }

    // §5.3: a state RESTORE is not a macro gesture. Every restore path
    // (session load, A/B switch, preset apply) notifies the macro listeners
    // as a side effect of landing the macro values, which would otherwise
    // queue a mapping pass that rewrites the nine managed parameters from the
    // curves — clobbering the exact off-curve values the restore just placed
    // and breaking raw-exact restoration (ADR-0007).
    //
    // Hold one of these across the WHOLE restore body. Dropping the armed
    // mapping at the end (what the restore paths used to do) is only
    // sufficient while the restore itself runs on the message thread: the
    // listeners fire during the restore, and if the 30 ms drain timer runs
    // between the arming and the drop — which it can whenever a host calls
    // `setStateInformation` off the message thread, as VST3 permits — the
    // mapping is applied MID-restore and no later abort can take it back.
    // The scope suppresses the drain for its whole lifetime and drops the
    // flag on the way out, so the guarantee no longer depends on the restore
    // out-racing the timer.
    //
    // Known, accepted property: the exit drops WHATEVER is armed — including a
    // genuine user gesture that flagged microseconds before the restore began.
    // That gesture's mapping is swallowed, not deferred. Harmless today (the
    // restore overwrites the managed set anyway, so applying the stale gesture
    // after it would be the §5.3 clobber this scope exists to prevent), but it
    // is a swallow by design, restated here so P4's gesture bracketing treats
    // it as a known property rather than a surprise.
    //
    // It is also the ONLY way to reach the abort (`abortPendingMapping` is
    // private): a new restore path cannot forget the step, because there is
    // no API that performs a restore without it.
    class ScopedRestore
    {
    public:
        explicit ScopedRestore (MacroEngine& e) noexcept : engine (e)
        {
            engine.restoreDepth.fetch_add (1, std::memory_order_relaxed);
        }

        ~ScopedRestore()
        {
            // Abort BEFORE the decrement, never after: between the two the
            // drain is still suppressed, so no window exists in which the
            // flag is armed and the guard is already down.
            engine.abortPendingMapping();
            engine.restoreDepth.fetch_sub (1, std::memory_order_relaxed);
        }

    private:
        MacroEngine& engine;
        JUCE_DECLARE_NON_COPYABLE (ScopedRestore)
    };

    // Deterministic flush for the headless tests (no message loop runs there):
    // applies a pending mapping now, on the calling (message) thread. Goes
    // through the same guard as the timer, so a test that flushes inside a
    // ScopedRestore models what the timer would really do there.
    void flushPendingMapping();

    // The POSTED drain, public for the same reason `drainTick` is: it is a
    // separate entry point into the same sequence, no message loop runs in the
    // headless tests, and an entry point no test can call is exactly how this
    // one drifted into running the mapping without the wrapper's drain in
    // front of it. Calling it directly is what the message queue would do.
    void handleAsyncUpdate() override;                             // message thread only

private:
    void parameterChanged (const juce::String&, float) override;   // any thread → flag only
    void timerCallback() override;                                 // message thread only
    void drainPendingMapping();                                    // message thread only
    void applyMapping();
    void setParam (const char* paramID, float denormalisedValue);

    void abortPendingMapping()
    {
        cancelPendingUpdate();
        mappingPending.store (false, std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState& apvts;

    // ATOMIC for the same reason `restoreDepth` below is, and it was the one
    // of the pair that was not. `isApplyingMacro()` is read by
    // `AnabasisAudioProcessor::parameterChanged`, which arrives on whichever
    // thread the host chose — including the audio thread. In the case that
    // MATTERS the read is synchronous on the thread that set the flag (a
    // mapping write re-entering the listener), so the discriminator was always
    // correct; a genuinely concurrent off-thread parameter change reading it
    // while the message thread flips it was nevertheless a formal data race,
    // whose worst outcome is one managed parameter wrongly detaching or
    // wrongly not. Relaxed: it gates a decision, it orders no payload.
    std::atomic<bool> applying { false };

    // parameterChanged can arrive on the AUDIO thread: APVTS calls its
    // listeners on whichever thread changed the parameter, and rule 5 makes
    // `withAutomatable(false)` advisory — a host may automate a macro anyway.
    // triggerAsyncUpdate() posts to the platform message queue, which takes a
    // lock (and on some platforms allocates), so calling it from there is a
    // REALTIME_AUDIO_POLICY hard-red-line violation. The audio thread now only
    // stores a flag; the message thread posts or drains it.
    std::atomic<bool> mappingPending { false };

    // Nesting depth of the ScopedRestore guards, not a bool: a restore path
    // may call another (a preset apply inside a session load), and a bool
    // would let the inner scope's exit re-open the window for the rest of the
    // outer one. Written from the restoring thread, read from the message
    // thread; relaxed for the same reason as `mappingPending` — it gates a
    // message-thread action and carries no payload.
    std::atomic<int> restoreDepth { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroEngine)
};
