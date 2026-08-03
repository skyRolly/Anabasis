#include "MacroEngine.h"
#include "PluginParameters.h"

MacroEngine::MacroEngine (juce::AudioProcessorValueTreeState& apvtsIn) : apvts (apvtsIn)
{
    apvts.addParameterListener (pid::loudness,  this);
    apvts.addParameterListener (pid::character, this);
    apvts.addParameterListener (pid::tone,      this);
    // The timer is NOT started here — see startDraining(). Everything the
    // listener above can do before then is set a flag; nothing reads the
    // std::function members until a drain runs.
}

void MacroEngine::startDraining()
{
    // Drains a flag set from a thread that may not post messages safely. 30 ms
    // is well inside the message-thread-rate mapping the macro layer promises
    // (ADR-0005 §5.2) and costs an atomic load when idle.
    //
    // Deliberately separate from the constructor: the tick reads `isDetached`
    // (through applyMapping) and `onDrainTick`, and the owner can only assign
    // those AFTER this object exists. Starting the timer in the constructor
    // left a window in which the message thread could run a tick while the
    // constructing thread was still assigning a std::function — and the
    // constructing thread is not promised to be the message thread (VST3 does
    // not promise it for `setStateInformation` either; KI-003). Owner calls
    // this once, after wiring.
    startTimer (30);
}

void MacroEngine::stopDraining()
{
    // Order matters: stop the repeating tick, then drop any single posted
    // update, then release the callbacks — after this returns, no path from
    // this object reaches the owner. `~MacroEngine` does the same two calls,
    // but by then the owner's members are already gone, which is the window
    // this closes. Residual, stated rather than implied: a tick already
    // EXECUTING on the message thread while this runs on another one is not
    // waited for — JUCE's Timer has no such join, and a host that destroys a
    // processor concurrently with its own message thread has a larger problem
    // than this callback.
    stopTimer();
    cancelPendingUpdate();
    onDrainTick = nullptr;
    isDetached  = nullptr;
}

MacroEngine::~MacroEngine()
{
    stopTimer();
    cancelPendingUpdate();
    apvts.removeParameterListener (pid::loudness,  this);
    apvts.removeParameterListener (pid::character, this);
    apvts.removeParameterListener (pid::tone,      this);
}

void MacroEngine::parameterChanged (const juce::String&, float)
{
    // May arrive on ANY thread, including the audio thread (rule 5:
    // withAutomatable(false) is advisory, so a host can automate a macro).
    // Never map here — and never post here either: triggerAsyncUpdate() locks
    // the platform message queue. A relaxed store is the whole audio-thread
    // cost; the message thread does the rest, so the engine stays
    // message-thread-only by construction (ADR-0011).
    mappingPending.store (true, std::memory_order_relaxed);

    if (juce::MessageManager::existsAndIsCurrentThread())
        triggerAsyncUpdate();     // UI/host edit on the message thread: map promptly
    // Otherwise the 30 ms timer drains it — no message posted from the caller.
}

void MacroEngine::handleAsyncUpdate()
{
    // The posted path, and it runs the SAME sequence as the timer — see
    // drainTick(). It called `drainPendingMapping()` alone for one round, which
    // is the fourth entry point the "all three agree" comment below did not
    // count: a macro id written on the message thread WITHOUT a gesture (host
    // automation of loudness/character/tone) posts here, and if a managed edit
    // had been delivered off-thread in the previous 30 ms its detach bit was
    // still staged — so the mapping overwrote the user's value, and the next
    // tick then marked that parameter detached at the value the macro had just
    // put there.
    drainTick();
}

void MacroEngine::timerCallback()
{
    drainTick();
}

void MacroEngine::drainTick()
{
    // ORDER IS THE CONTRACT, and it is the opposite of what this used to do.
    // The wrapper's bits go FIRST: they decide the detach mask, and the mapping
    // below consults that mask through `isDetached` to know which managed
    // parameters it may write. Mapping first meant that when a macro gesture
    // and its value change both arrived off the message thread — the only way
    // the two can reach one tick together — the mapping SKIPPED a parameter the
    // gesture was about to re-engage, and the mask was cleared a moment later:
    // the parameter then read as re-engaged while still holding the user's
    // off-curve value, and nothing re-armed the mapping, so it stayed there
    // until some later macro move. §5.3's rule is "the next macro-knob gesture
    // re-engages ALL detached params"; that is only true if the re-engage is
    // visible to the pass that lands the curve.
    //
    // This is the same precedence the wrapper's drain applies INTERNALLY
    // (detach bits, then the re-engage clears over them), and the same one the
    // message-thread gesture path gets for free — there `drainDetachBitsSoon`
    // clears synchronously at gesture begin, before any mapping write.
    //
    // EVERY trigger routes through this function — the 30 ms timer, the posted
    // `handleAsyncUpdate`, and `flushPendingMapping` — because the previous
    // revision fixed the order here and left `handleAsyncUpdate` calling
    // `drainPendingMapping()` alone, then claimed in this very comment that
    // "all three now agree". Counting the paths in prose is how the fourth one
    // was missed; there is now one sequence and three ways to ask for it.
    //
    // A restore in flight suppresses the WHOLE tick, not just the mapping.
    // The guard used to sit one level down, in `drainPendingMapping`, so the
    // wrapper's half ran anyway — and the wrapper's half writes
    // `liveDetachMask`, a plain `juce::StringArray` that the restore is itself
    // replacing. On the message thread that is only wasted work (the wrapper
    // stages no bits while `isRestoring()`, and every restore path ends at
    // `replaceDetachMask`, which drops whatever was staged before it); on the
    // off-message-thread `setStateInformation` a host may perform, it made the
    // tick a SECOND concurrent writer of that array, widening the window
    // KI-003 records rather than leaving it as found. Deferring is
    // outcome-neutral — the bits are dropped by `replaceDetachMask` exactly as
    // they would have been had they landed and then been overwritten — and it
    // is the same rule the mapping half already followed: a restore is not a
    // gesture, so a gesture racing one belongs to the state being replaced.
    if (restoreDepth.load (std::memory_order_relaxed) > 0)
        return;

    // RE-ENTRANCY, stated because routing every trigger through here created
    // it and neither site said so. `onDrainTick` is the WRAPPER's detach
    // drain, so `refreshMapping()` — called by `applyFactoryPreset` and
    // `resetToMacro` — now runs that drain synchronously from inside a wrapper
    // method. Safe, and for a reason rather than by luck: both of those
    // callers reach `replaceDetachMask()` first, which clears the staged bits
    // AND the re-engage flag, so the drain below them finds nothing to apply.
    // The mapping's own `setValueNotifyingHost` writes cannot re-enter either:
    // they land in `AnabasisAudioProcessor::parameterChanged`, which returns
    // before `drainDetachBitsSoon()` because `isApplyingMacro()` is true for
    // the whole of `applyMapping`. Break either of those two and this becomes
    // a re-entrant mask write in the middle of a preset apply.
    if (onDrainTick)
        onDrainTick();
    drainPendingMapping();
}

void MacroEngine::flushPendingMapping()
{
    cancelPendingUpdate();
    drainTick();
}

void MacroEngine::drainPendingMapping()
{
    // The restore guard is in `drainTick`, the ONLY caller, and covers this
    // half too. What it buys here, unchanged from when it sat on this line:
    // the flag is left ARMED and the restore's ScopedRestore drops it on the
    // way out, which is the §5.3 outcome — "a restore is not a macro gesture"
    // — reached without this thread writing the nine managed parameters
    // underneath it. Consuming the flag under the guard instead would be the
    // same outcome by a longer route, but would make the drain's behaviour
    // depend on which of the two ran first.
    if (mappingPending.exchange (false, std::memory_order_relaxed))
        applyMapping();
}

void MacroEngine::applyMapping()
{
    const float l = apvts.getRawParameterValue (pid::loudness)->load() * 0.01f;
    const float c = apvts.getRawParameterValue (pid::character)->load();
    const float t = apvts.getRawParameterValue (pid::tone)->load();

    applying.store (true, std::memory_order_relaxed);
    // A detached parameter is the user's until re-engage (§5.3): the mapping
    // skips it entirely rather than writing and hoping nobody noticed.
    setParam (pid::limGain,       macro_curves::limGainDb (l));
    setParam (pid::compThreshold, macro_curves::compThresholdDb (l));
    setParam (pid::compRatio,     macro_curves::compRatio (l));
    setParam (pid::clipDrive,     macro_curves::clipDriveDb (l));
    setParam (pid::clipShape,     macro_curves::clipShape (l));
    setParam (pid::colourDepth,   macro_curves::colourDepthPct (l, c));
    setParam (pid::dynTilt,       macro_curves::dynTiltDb (l));
    setParam (pid::eqTilt,        macro_curves::eqTiltDb (t));
    setParam (pid::colourTone,    macro_curves::colourTone (t));
    applying.store (false, std::memory_order_relaxed);
}

void MacroEngine::setParam (const char* paramID, float denormalisedValue)
{
    if (isDetached != nullptr && isDetached (paramID))
        return;
    if (auto* p = apvts.getParameter (paramID))
    {
        const float norm = p->getNormalisableRange().convertTo0to1 (
                               p->getNormalisableRange().snapToLegalValue (denormalisedValue));
        if (std::abs (p->getValue() - norm) > 1.0e-6f)   // no-op writes stay silent
            p->setValueNotifyingHost (norm);
    }
}
