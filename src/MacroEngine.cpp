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
    // A restore is in flight: leave the flag ARMED and do nothing. The
    // restore's ScopedRestore drops it on the way out, which is the §5.3
    // outcome — "a restore is not a macro gesture" — reached without this
    // thread writing the nine managed parameters underneath it. Consuming the
    // flag here instead would be the same outcome by a longer route, but it
    // would make the drain's behaviour depend on which of the two ran first.
    if (restoreDepth.load (std::memory_order_relaxed) > 0)
        return;

    if (mappingPending.exchange (false, std::memory_order_relaxed))
        applyMapping();
}

void MacroEngine::applyMapping()
{
    const float l = apvts.getRawParameterValue (pid::loudness)->load() * 0.01f;
    const float c = apvts.getRawParameterValue (pid::character)->load();
    const float t = apvts.getRawParameterValue (pid::tone)->load();

    applying = true;
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
    applying = false;
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
