#include "MacroEngine.h"
#include "PluginParameters.h"

MacroEngine::MacroEngine (juce::AudioProcessorValueTreeState& apvtsIn) : apvts (apvtsIn)
{
    apvts.addParameterListener (pid::loudness,  this);
    apvts.addParameterListener (pid::character, this);
    apvts.addParameterListener (pid::tone,      this);
    // Drains a flag set from a thread that may not post messages safely. 30 ms
    // is well inside the message-thread-rate mapping the macro layer promises
    // (ADR-0005 §5.2) and costs an atomic load when idle.
    startTimer (30);
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
    if (mappingPending.exchange (false, std::memory_order_relaxed))
        applyMapping();
}

void MacroEngine::timerCallback()
{
    if (mappingPending.exchange (false, std::memory_order_relaxed))
        applyMapping();
}

void MacroEngine::flushPendingMapping()
{
    cancelPendingUpdate();
    if (mappingPending.exchange (false, std::memory_order_relaxed))
        applyMapping();
}

void MacroEngine::applyMapping()
{
    const float l = apvts.getRawParameterValue (pid::loudness)->load() * 0.01f;
    const float c = apvts.getRawParameterValue (pid::character)->load();
    const float t = apvts.getRawParameterValue (pid::tone)->load();

    applying = true;
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
    if (auto* p = apvts.getParameter (paramID))
    {
        const float norm = p->getNormalisableRange().convertTo0to1 (
                               p->getNormalisableRange().snapToLegalValue (denormalisedValue));
        if (std::abs (p->getValue() - norm) > 1.0e-6f)   // no-op writes stay silent
            p->setValueNotifyingHost (norm);
    }
}
