#include "PluginProcessor.h"
#include "gui/PluginEditor.h"
#include "dsp/Latency.h"

namespace
{
    constexpr int kSchemaVersion = 1;   // ADR-0007: explicit from day one
}

AnabasisAudioProcessor::AnabasisAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANABASIS", createAnabasisLayout (&ceilingUnit))
{
    cached.resolve (apvts);
    // The Ceiling's suffix follows the live true-peak mode (ADR-0015). Wired
    // HERE and not in the layout because the layout builds `ceiling` (row 6)
    // long before `truePeakMode` (row 33) exists. `ceilingUnit` is declared
    // ahead of `apvts` in the header, so it is already constructed when the
    // layout captures its address (the member's comment states what that
    // ordering does and does not prove); until this store lands the suffix is
    // the fallback " dB".
    ceilingUnit.truePeakRaw.store (apvts.getRawParameterValue (pid::truePeakMode),
                                   std::memory_order_relaxed);
    macroEngine   = std::make_unique<MacroEngine> (apvts);
    presetManager = std::make_unique<PresetManager> (apvts, internalState);

    // Two of ADR-0004 item 5's recompute triggers wire here: the three
    // latency-input onChanged callbacks. prepareToPlay and setNonRealtime()
    // are the others. The callback runs on whichever thread mutated the
    // ValueTree (message thread in practice); updateLatency never touches
    // audio-thread state, so this is safe from any non-audio thread.
    internalState.onLatencyInputChanged = [this] { updateLatency(); };

    // The fresh state IS the "Default" preset (factory index 0, empty
    // override table). Named BEFORE the default slot is captured so both
    // slots open carrying it; the dirty baseline is seeded right after, so
    // an untouched instance reads clean and the first edit stars.
    livePresetName = "Default";
    defaultSlot = saveSlotFromLive();   // pristine defaults (missing-AB read rule)
    storedSlot  = defaultSlot.createCopy();   // slot B starts as a copy of defaults
    presetBaseline       = presetShapeFromLive();
    storedPresetBaseline = presetBaseline.createCopy();

    // §5.3 detach discriminator (ADR-0005's P5 half — see the header block).
    // The mapper asks the wrapper, never the reverse: the mask is per-slot
    // serialized state and lives here.
    // `juce::StringRef`, NOT `juce::String`. `juce::String` has no small-string
    // optimisation, so constructing one here heap-allocated on every call — and
    // this is called once per managed parameter by `MacroEngine::setParam`, i.e.
    // nine allocations per mapping pass, ~300/s at the 30 ms tick. Harmless on
    // the message thread and well inside budget, but the comments around the
    // re-engage path describe that pass as costing "nine comparisons", which was
    // an understatement rather than a description. `StringRef` wraps the literal
    // without owning it (JUCE_STRING_UTF_TYPE is 8 on every platform this
    // builds for, so it stores a bare `CharPointer_UTF8`), and
    // `StringArray::contains` takes one directly. The claim is now true.
    macroEngine->isDetached = [this] (const char* id)
    { return liveDetachMask.contains (juce::StringRef (id)); };
    // Off-message-thread detach/re-engage bits land on the MacroEngine's
    // existing 30 ms tick rather than through a message post of our own
    // (drainDetachBitsSoon explains why that route is closed).
    macroEngine->onDrainTick = [this] { handleAsyncUpdate(); };
    // The undo gesture bookkeeping keys one bit per parameter index; ADR-0010
    // freezes the surface at 49, well inside the word.
    jassert (getParameters().size() <= kMaxCountedGestureIndex);
    addListener (this);                       // gesture begin/end
    // The MANAGED nine only. The three macro ids were registered here too and
    // `parameterChanged` discarded every one of those callbacks on its first
    // line (`managedIndexOf` fails for a macro) — three registrations that read
    // as load-bearing and were not, which is how a future edit comes to assume
    // macro writes reach this handler. `MacroEngine` is the macros' listener;
    // the wrapper hears them through the GESTURE callbacks instead, which is
    // where §5.3's re-engage rule actually lives.
    for (const char* id : managed_params::ids)
        apvts.addParameterListener (id, this);

    // LAST STATEMENT IN THE CONSTRUCTOR, deliberately, and this is the third
    // time this object's lifecycle has had to be made structural rather than
    // ordered. `startDraining()` used to run right after the two callbacks were
    // assigned, several statements above — with a comment saying "only now may
    // the tick that reads them run", which was true of the callbacks and not of
    // everything else the tick reaches. `onDrainTick` is
    // `AnabasisAudioProcessor::handleAsyncUpdate`, which drains the staged
    // detach bits, can call `replaceDetachMask` and can land a mapping pass —
    // and a mapping pass writes parameters, whose callbacks the listener
    // registrations above had not yet subscribed to. A tick arriving in that
    // window would have run a macro apply the wrapper could not hear.
    //
    // Nothing could deliver such a tick today: `juce::Timer` fires from the
    // message loop, which cannot run inside a constructor executing on the
    // message thread. That is exactly the "safe by ordering" argument the
    // startDraining/stopDraining split exists to stop relying on — the same
    // reasoning that put `stopDraining()` first in the destructor and, since
    // round 45, inside `~MacroEngine` itself. Arming last costs nothing and
    // makes the guarantee a property of this function rather than of the
    // platform's dispatch rules.
    macroEngine->startDraining();
}

AnabasisAudioProcessor::~AnabasisAudioProcessor()
{
    // FIRST, before any member is destroyed — see the declaration.
    macroEngine->stopDraining();

    // Symmetric with the constructor, and deliberately explicit rather than
    // order-dependent. It IS safe implicitly today — `apvts` is declared above
    // every member these callbacks touch, so it outlives them, and nothing
    // changes a parameter during teardown — but "safe because of declaration
    // order" is the argument the startDraining/stopDraining split was written
    // to stop relying on. Deregistering says what the destructor guarantees
    // instead of leaving a reader to re-derive it.
    removeListener (this);
    for (const char* id : managed_params::ids)
        apvts.removeParameterListener (id, this);
}

// The three §5.3 conditions meet here. Gesture callbacks arrive with a raw
// parameter INDEX; the managed set is matched by ID so a layout reorder can
// never silently re-key the discriminator.
static int managedIndexOf (const juce::String& id)
{
    for (int i = 0; i < managed_params::kCount; ++i)
        if (id == managed_params::ids[i])
            return i;
    return -1;
}

// The §7 change test compares what an undo could RESTORE (ADR-0018): the
// view-tier entries never travel with a slot restore — `applySlotToLive`
// pins them — so both sides of the compare have them normalised away here.
// Without this, a monitor toggle clicked while a knob drag was open minted a
// step whose restore changed nothing. `advancedMode` is deliberately NOT
// stripped: its diff is restorable (the undo path adopts it), so it must
// keep minting steps.
static juce::ValueTree strippedForUndoCompare (const juce::ValueTree& slot)
{
    auto copy = slot.createCopy();
    auto params = copy.getChildWithName ("ANABASIS");
    if (! params.isValid())
        return copy;
    for (int i = 0; i < params.getNumChildren(); ++i)
    {
        auto node = params.getChild (i);
        if (node.hasType ("PARAM") && isViewTierParam (node.getProperty ("id").toString()))
        {
            node.setProperty ("value", 0.0, nullptr);
            node.removeProperty ("raw", nullptr);
        }
    }
    return copy;
}

void AnabasisAudioProcessor::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*,
                                                                        int parameterIndex)
{
    // §7 undo: the FIRST open gesture snapshots the pre-state the eventual
    // step will restore. Message thread only — an off-thread gesture (host
    // UI) skips this and its edit folds silently, the automation rule.
    //
    // Open drags are tracked as a BITMASK keyed by parameter index, not as a
    // bare count, because VST3 gesture threading is host-defined (the same
    // premise this class's off-thread degradation path rests on) and the two
    // asymmetries a count cannot survive are opposites:
    //   • begin off the message thread, end on it — a bare `--` closed a
    //     DIFFERENT, still-open drag, pushing its step mid-gesture and clearing
    //     the snapshot, so its real end pushed nothing. Only a bit this handler
    //     set can be cleared, so a foreign end matches nothing.
    //   • begin on the message thread, end off it — the end below clears the
    //     bit on ANY thread (see there), so the mask cannot leak. A count
    //     guarded by the thread test leaked one open drag for ever, and undo
    //     then never fired again for the rest of the session.
    // Only message-thread begins set a bit: an off-thread gesture is invisible
    // to undo by design (it folds into the automation path), and letting it
    // occupy the mask would suppress a concurrent real drag's step.
    //
    // NOTE the deliberate asymmetry with `managedGestureBits` below, which is
    // set and cleared on ANY thread: an off-thread drag DOES detach but does
    // NOT produce an undo step. The two masks answer different questions and
    // §5.3/§7 word them differently on purpose — detachment keys on "the change
    // was gesture-bracketed", which is true whoever delivered it, while an undo
    // step keys on a message-thread drag, because pushing one means copying
    // ValueTrees and that may not happen off the message thread. Stated here
    // because the two lines sit three apart and look like an oversight.
    // ADR-0018: the view-tier toggles (bypass and the two monitor functions)
    // never travel with an undo restore — `applySlotToLive` pins them — so
    // their clicks must not ARM the undo machinery either. Before this gate,
    // a BYPASS click snapshotted the full tree, the end-compare saw the
    // bypass diff, and a step was pushed whose restore changed nothing: one
    // Undo press eaten per click. (`advancedMode` deliberately passes — its
    // step is real since ADR-0018.) The sibling avoids the whole class by
    // never listening to its view params; this listener hears everything, so
    // the exclusion is spelled here.
    const auto* pw = dynamic_cast<juce::AudioProcessorParameterWithID*> (
        getParameters()[parameterIndex]);
    const bool undoEligible = pw == nullptr || ! isViewTierParam (pw->getParameterID());

    if (undoEligible
        && juce::MessageManager::existsAndIsCurrentThread()
        && parameterIndex >= 0 && parameterIndex < kMaxCountedGestureIndex)
    {
        const uint64_t bit = 1ull << parameterIndex;
        // Arming on 0 → non-zero is what makes one drag (or several
        // overlapping ones) exactly one step.
        if (openGestureBits.fetch_or (bit, std::memory_order_relaxed) == 0)
        {
            syncHistory();               // message thread — see the declaration
            gesturePreState = saveSlotFromLive();
        }
    }

    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (
            getParameters()[parameterIndex]))
    {
        const auto id = p->getParameterID();
        if (const int m = managedIndexOf (id); m >= 0)
            managedGestureBits.fetch_or (1u << m, std::memory_order_relaxed);
        else if (id == pid::loudness || id == pid::character || id == pid::tone)
        {
            // §5.3: the NEXT macro-knob gesture re-engages everything — the
            // clear must land before the gesture's mapping writes, and both
            // run on the message thread, so the drain below is ordered right.
            //
            // ARM THE MAPPING TOO, and it is the same invariant round 30 fixed
            // for the tick path: a re-engage that the curve-landing pass never
            // sees leaves a parameter reading as re-engaged while it still
            // holds the user's off-curve value. A gesture that moves the knob
            // arms the mapping through the macro's own listener; a gesture
            // that moves NOTHING — press and release, or a drag returned to
            // where it started — armed nothing, so the mask cleared and the
            // curve never re-landed. `resetToMacro()`, the only other
            // re-engagement verb, clears the mask AND re-lands; these two are
            // now the same rule rather than two readings of it. Inert when
            // nothing was detached: `MacroEngine::setParam` skips writes that
            // would not change the value, so the pass costs nine mask
            // comparisons — allocation-free since the `isDetached` lambda takes
            // a `StringRef` (see its definition; it used to build a
            // `juce::String` per call, which made this sentence untrue).
            pendingReengage.store (true, std::memory_order_relaxed);
            macroEngine->armMapping();         // any thread — an atomic store
            drainDetachBitsSoon();             // never posts off-thread — see there
        }
    }
}

void AnabasisAudioProcessor::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*,
                                                                      int parameterIndex)
{
    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (
            getParameters()[parameterIndex]))
        if (const int m = managedIndexOf (p->getParameterID()); m >= 0)
            managedGestureBits.fetch_and (~(1u << m), std::memory_order_relaxed);

    // The bit is cleared on WHICHEVER thread the end arrives on — that is the
    // half a thread-guarded count could not do, and its absence stranded an
    // open drag for ever (undo then never fired again). Clearing a bit is a
    // lock-free store; only the ValueTree work below is message-thread-gated,
    // which is the same split the detach discriminator uses.
    const uint64_t bit = parameterIndex >= 0 && parameterIndex < kMaxCountedGestureIndex
                             ? 1ull << parameterIndex : 0ull;
    if (bit == 0)
        return;
    const uint64_t prevOpen = openGestureBits.fetch_and (~bit, std::memory_order_relaxed);

    // Push only when THIS end closed the last open drag (`prevOpen == bit`):
    // an end for a bit we never set — a begin delivered off-thread — matches
    // nothing, and overlapping drags collapse to one step at the last close.
    // An end that arrives off-thread still clears above but pushes nothing:
    // that step is lost, which is the documented automation-path degradation,
    // and the next message-thread begin re-arms from 0 rather than inheriting
    // a stale snapshot.
    if (prevOpen == bit && juce::MessageManager::existsAndIsCurrentThread())
    {
        // The snapshot's validity is tested AFTER the reconcile, not with it:
        // a load that landed mid-drag has already invalidated this pre-state,
        // and `syncHistory()` is what drops it. Testing first would have read
        // a snapshot belonging to the previous session.
        syncHistory();
        // One step per completed drag, and only if something an undo could
        // RESTORE changed — an aborted gesture (press, no move) pushes
        // nothing, and neither does a drag whose only diff is a view-tier
        // toggle clicked mid-gesture (ADR-0018: `applySlotToLive` pins those
        // entries, so their diff is unrestorable and must not mint a step).
        if (gesturePreState.isValid()
            && ! strippedForUndoCompare (gesturePreState)
                   .isEquivalentTo (strippedForUndoCompare (saveSlotFromLive())))
            pushUndoStep (gesturePreState);
        gesturePreState = {};
    }
}

// The ONE place a StateSet joins either stack, so `kUndoCap` is applied to
// whichever it is. `undo()`/`redo()` push onto the opposite stack too, and the
// cap was written here alone: bounded in PRACTICE, because a redo entry can
// only come from popping a capped undo stack so the pair never exceeds the cap
// in total — but that is an invariant living in two places, and a future path
// that pushed a redo entry directly would grow without limit. One function,
// one bound.
template <typename Stack, typename Entry>
static void pushCapped (Stack& stack, Entry entry, int cap)
{
    stack.add (std::move (entry));
    while (stack.size() > cap)
        stack.remove (0);            // oldest first: the cap trims history, not the present
}

// §6.1 Copy, ADR-0018 semantics (the sibling's #12 rule, adopted verbatim):
// the destination slot's PRE-COPY state becomes an undo entry on the
// DESTINATION's stack, its older history is kept, and only its redo line is
// cleared (a new action invalidates redo, same as any edit). Undoing on the
// copied-into slot therefore reverts the Copy; further undos walk the
// destination's own pre-copy history — coherent because entries are absolute
// SLOT snapshots, not diffs. The 0.1.0 behaviour (clear both stacks, "as a
// load does") argued the old entries described states the slot no longer has
// — true only because the copy itself was not a step; making it one makes
// them reachable again, which is the sibling's resolution and the owner's
// 0.1.1 directive.
//
// The entry pairs the destination's OWN dirty datum (`storedPresetBaseline`),
// not the active slot's live one — `pushUndoStep` targets the active stack
// and pairs the live baseline, so it cannot be reused here.
//
// Still no duck and no engine involvement: nothing audible changes.
void AnabasisAudioProcessor::copySlotToOther()
{
    syncHistory();                       // epoch reconcile before any stack touch
    const int other = 1 - activeSlot;
    // …and `advancedMode` comes from LIVE, not from `storedSlot` — see
    // `slotWithLiveAdvancedMode`. Without that pin, undoing a Copy could
    // resize the editor, which is the one thing ADR-0018 says Copy and its
    // undo must never do.
    UndoEntry pre { slotWithLiveAdvancedMode (storedSlot),
                    storedPresetBaseline.createCopy() };
    auto liveSlot = saveSlotFromLive();

    // ONLY IF THE DESTINATION ACTUALLY CHANGES. Press Copy twice with no edit
    // between and the second one overwrites the destination with what it
    // already holds — the entry it would push restores the state it replaces,
    // so one Undo press on that slot appears to do nothing. That is precisely
    // the dead step ADR-0018 §Decision 4 set out to remove from the gesture
    // path, arriving by a different route, so it takes the same answer: the
    // gesture path's change test, `strippedForUndoCompare` on both sides,
    // which normalises the view-tier entries an undo could not restore anyway.
    // The dirty datum is compared too — it is the other half of what the entry
    // would restore, and a baseline that moved is a real difference even when
    // the parameter surface did not.
    //
    // The redo line is left alone in that case for the same reason nothing is
    // pushed: a Copy that changes nothing is not a new action, and the gesture
    // path likewise clears no redo when its diff is empty.
    if (! strippedForUndoCompare (pre.slot)
             .isEquivalentTo (strippedForUndoCompare (liveSlot))
        || ! pre.baseline.isEquivalentTo (presetBaseline))
    {
        pushCapped (undoStacks[other], std::move (pre), kUndoCap);
        redoStacks[other].clear();
    }
    storedSlot = std::move (liveSlot);
    // `createCopy()`, for the reason `undo()`/`redo()` carry: assigning a
    // `juce::ValueTree` shares the refcounted node, so the two slots' dirty
    // data would be ONE tree until the next wholesale replacement. Harmless
    // while a baseline is only ever replaced, never edited in place — which
    // is true of every writer today — and a trap the moment one is not, since
    // an edit made "for slot A" would appear in B. `storedSlot` above needs
    // no such call: `saveSlotFromLive()` returns a freshly built tree that
    // nothing else holds.
    storedPresetBaseline = presetBaseline.createCopy();
}

void AnabasisAudioProcessor::pushUndoStep (juce::ValueTree preState)
{
    syncHistory();
    // `presetBaseline` is read HERE rather than passed in, and every caller
    // makes that correct: a gesture does not touch the datum, and the preset
    // applies push BEFORE they replace it, so what this reads is always the
    // baseline that belonged beside `preState`.
    pushCapped (undoStacks[activeSlot],
                UndoEntry { preState.createCopy(), presetBaseline.createCopy() }, kUndoCap);
    redoStacks[activeSlot].clear();      // a new edit invalidates the redo line
}

// §2.8: an undo step is a bulk swap exactly like an A/B switch or a preset
// apply — DSP_POLICY invariant 8's click-free enumeration names it explicitly
// (ADR-0004). It restores a whole StateSet, so it can rewire the discrete
// stages (EQ position, colour model, OS factor), and since ADR-0014 it also
// STAGES the frozen-trim vector, which is only ever applied at the silent
// bottom. Without the request an undo that happens to move no rewire never
// reaches a bottom at all: the staged vector then sat pending indefinitely and
// was injected at the next unrelated duck, into whatever slot was live by then.
void AnabasisAudioProcessor::undo()
{
    syncHistory();
    auto& stack = undoStacks[activeSlot];
    if (stack.isEmpty())
        return;
    pushCapped (redoStacks[activeSlot],
                UndoEntry { saveSlotFromLive(), presetBaseline.createCopy() }, kUndoCap);
    const auto prev = stack.removeAndReturn (stack.size() - 1);
    const MacroEngine::ScopedRestore guard (*macroEngine);   // §5.3: not a gesture
    engine.requestForcedDuck();
    applySlotToLive (prev.slot, /*adoptAdvanced*/ true);     // ADR-0018: ADV undoes
    // …and the datum that described it. `applySlotToLive` restores
    // `presetName` from the StateSet, so restoring one without the other left
    // the top bar comparing a previous preset's state against the applied
    // preset's baseline and rendering the mark for neither.
    //
    // `createCopy()`, not a plain assignment: `juce::ValueTree` assignment
    // shares the refcounted node, so the live datum and the history entry that
    // supplied it would be ONE tree. Nothing edits a baseline in place today —
    // both are only ever replaced wholesale, by `presetShapeFromLive()` or by a
    // history entry — so the alias is invisible now, and it is exactly the kind
    // of invisible that a later in-place edit turns into an undo entry silently
    // rewriting itself. The copy is a ~46-node clone on a user action.
    presetBaseline = prev.baseline.createCopy();
}

void AnabasisAudioProcessor::redo()
{
    syncHistory();
    auto& stack = redoStacks[activeSlot];
    if (stack.isEmpty())
        return;
    pushCapped (undoStacks[activeSlot],
                UndoEntry { saveSlotFromLive(), presetBaseline.createCopy() }, kUndoCap);
    const auto next = stack.removeAndReturn (stack.size() - 1);
    const MacroEngine::ScopedRestore guard (*macroEngine);
    engine.requestForcedDuck();                              // §2.8, as undo()
    applySlotToLive (next.slot, /*adoptAdvanced*/ true);     // ADR-0018, as undo()
    presetBaseline = next.baseline.createCopy();             // paired, as undo()
}

void AnabasisAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    const int m = managedIndexOf (parameterID);
    if (m < 0)
        return;                                   // macros route through MacroEngine —
                                                  // and are no longer REGISTERED here,
                                                  // so this line now guards only an id
                                                  // a future registration might add
    if ((managedGestureBits.load (std::memory_order_relaxed) & (1u << m)) == 0)
        return;                                   // ungestured: automation/restore — never detaches
    if (macroEngine->isApplyingMacro() || macroEngine->isRestoring())
        return;
    pendingDetachBits.fetch_or (1u << m, std::memory_order_relaxed);
    drainDetachBitsSoon();
}

// The bits are set from whichever thread APVTS/the host delivers the callback
// on, and only the message thread may turn them into `liveDetachMask`. On the
// message thread that is immediate; OFF it, the drain WAITS for the MacroEngine
// tick — deliberately, and this is the one line that must not become
// `triggerAsyncUpdate()`. Posting to the platform message queue takes a lock
// and on some platforms allocates, so a callback delivered on the audio thread
// would put both inside `processBlock`: a REALTIME_AUDIO_POLICY hard red line.
// `MacroEngine::parameterChanged` refused to post for exactly this reason and
// leans on its 30 ms timer instead; the detach bits now ride the same tick
// rather than opening a second, riskier route to the same place. Cost of the
// wait: the badge and the serialized mask lag by up to 30 ms on a host that
// delivers gestures off-thread — display latency, never a wrong value.
void AnabasisAudioProcessor::drainDetachBitsSoon()
{
    // The RESTORE guard, which this entry point did not have. `drainTick`
    // suppresses both halves of the drain inside a `ScopedRestore` — the
    // mapping AND the wrapper's detach drain — because a restore is replacing
    // `liveDetachMask` wholesale and a drain landing in the middle of it is a
    // second writer of the array the restore is installing. This function
    // reaches the same `handleAsyncUpdate()` directly, so it was the one path
    // where that suppression depended on nobody calling it during a restore
    // rather than on the call refusing.
    //
    // It IS unreachable today: `parameterChanged` returns before getting here
    // when `isRestoring()`, and the macro gesture-begin path cannot run
    // concurrently with a message-thread restore. That is a reachability
    // argument about two callers, and reachability arguments are what this PR
    // has spent its rounds converting into structure — the third caller is the
    // one that would not know. Behaviour is unchanged for every path that
    // exists now; what changes is that a future one cannot get it wrong.
    if (macroEngine->isRestoring())
        return;
    if (juce::MessageManager::existsAndIsCurrentThread())
        handleAsyncUpdate();
}

void AnabasisAudioProcessor::handleAsyncUpdate()
{
    // Message thread. §5.3: "the next macro-knob gesture re-engages ALL
    // detached params", so a macro gesture that raced a detach WINS — which
    // means the detaches are applied FIRST and the re-engage clears over them,
    // not the other way round. The two orders differ only when both are
    // pending in the same drain, and on a host that delivers callbacks on the
    // message thread that never happens (drainDetachBitsSoon runs one per
    // callback). Off-thread they coexist inside one 30 ms tick — and the
    // previous order let the DETACH win there, leaving a parameter detached
    // through the very gesture the rule says re-engages it. The comment here
    // claimed the correct precedence while the code did the opposite.
    if (auto bits = pendingDetachBits.exchange (0, std::memory_order_relaxed))
        for (int i = 0; i < managed_params::kCount; ++i)
            if ((bits & (1u << i)) != 0)
                liveDetachMask.addIfNotAlreadyThere (managed_params::ids[i]);
    if (pendingReengage.exchange (false, std::memory_order_relaxed))
        liveDetachMask.clear();
}

juce::AudioProcessorParameter* AnabasisAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (pid::bypass);
}

bool AnabasisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo out always; mono OR stereo in — the sibling's contract, restored
    // here for KI-009. Refusing mono→stereo forced hosts with a mono source to
    // negotiate stereo→stereo and feed whatever their convention puts on the
    // two input pins — several put the signal on ONE pin and silence on the
    // other, and this chain is strictly dual-mono (the comp/limiter "link"
    // shares only the detector LEVEL), so a silent input pin is a silent
    // output channel in both modes. Accepting mono and duplicating it below
    // removes that negotiation entirely.
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono();
}

void AnabasisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // NOTHING of the wrapper's ValueTree state is touched here, and that is a
    // requirement rather than an accident. `prepareToPlay` is a host callback
    // JUCE does not promise on the message thread (the same premise
    // THREADING_POLICY's PDC amendment and KI-003 rest on), while the editor
    // polled `saveSlotFromLive()` — which reads `liveFrozenTrims` and
    // `createCopy()`s it — continuously. (That poll is now the far narrower
    // `presetShapeFromLive()`, which touches no ValueTree at all; the rule here
    // is unchanged, because `getStateInformation` and the A/B swap still reach
    // the mirror and neither is promised on the message thread.) Round 40 rescued
    // the frozen latch by copying it into that mirror HERE, which put an
    // unsynchronised write to a non-thread-safe `juce::ValueTree` opposite a
    // continuous reader, with both sides gated on Freeze being ON so the
    // windows coincided exactly. The latch is now retained where it was already
    // lock-free — `AdaptiveEngine`'s retained trim set, which `reset()` does not
    // clear — so no thread crossing is added to rescue it.
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    grHistoryRing.reset();
    dbTpMaxHold = samplePeakMaxHold = -144.0f;
    // Publish the cleared values too, not just the state behind them: without
    // this the six meter atomics keep the previous session's readings until a
    // block completes — and indefinitely if the host prepares without ever
    // processing (a rate change while stopped, a plugin rescan).
    publishSilentMeters();
    updateLatency();
}

// The published meter atomics, cleared — ONE list, because three sites need
// exactly this and two of them had grown their own copy. (Ten since the
// ADR-0020 stats row; the count is deliberately not repeated in prose here or
// at the callers, because it was wrong within two commits of being written
// the first time — the LIST is the count.) Relaxed stores, so it is callable
// from any thread: `prepareToPlay` (host), the block-top meter-reset consume
// (audio) and `setStateInformation` (whichever thread the host restores on)
// all use it. It deliberately does NOT touch the two audio-thread max-holds,
// which stay with the two callers that own that thread.
void AnabasisAudioProcessor::publishSilentMeters() noexcept
{
    pubLufsM.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubLufsS.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubLufsI.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubDbTpMax.store (-144.0f, std::memory_order_relaxed);
    pubPlr.store (0.0f, std::memory_order_relaxed);
    pubGrDb.store (0.0f, std::memory_order_relaxed);
    pubPeakMaxDb.store (-144.0f, std::memory_order_relaxed);
    pubRmsDb.store (anabasis::RmsMeter::kSilentDb, std::memory_order_relaxed);
    pubLufsIUngated.store (anabasis::LoudnessMeter::kSilentLufs, std::memory_order_relaxed);
    pubLra.store (anabasis::LoudnessMeter::kNoLra, std::memory_order_relaxed);
}

void AnabasisAudioProcessor::setNonRealtime (bool isNonRealtime) noexcept
{
    AudioProcessor::setNonRealtime (isNonRealtime);
    nonRealtimeFlag.store (isNonRealtime, std::memory_order_relaxed);
    // The realtime→offline transition is a PDC recompute trigger in its own
    // right (ADR-0004 item 5): at Force Max the reported figure uses the
    // forced 16x factor, and this is the only callback guaranteed to fire.
    updateLatency();
}

void AnabasisAudioProcessor::updateLatency()
{
    // Snapshot the inputs the predictor needs; const and race-free, never
    // mutating audio-thread state (THREADING_POLICY forbidden-access rule).
    anabasis::EngineParameters p;
    p.oversample      = internalState.oversampleFactor();
    p.osPhase         = internalState.osPhaseMode();
    p.forceMaxOffline = internalState.forceMaxOffline();
    p.nonRealtime     = nonRealtimeFlag.load (std::memory_order_relaxed);

    // The 48 kHz fallback covers calls before the first prepareToPlay (a
    // setStateInformation or an int_ change can arrive first), so the figure
    // reported from an unprepared state is a placeholder — 480 samples
    // regardless of the host's eventual rate. prepareToPlay re-reports with
    // the real rate before any audio runs, and wrappers set the rate before
    // calling it, so no host compensates with the placeholder. A future PDC
    // test must therefore assert the reported value only AFTER a prepare.
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    setLatencySamples (anabasis::predictLatencySamples (p, sr));
}

void AnabasisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;   // the single FTZ/DAZ mechanism (§1.4)

    // §2.9 meter-hold reset, consumed BEFORE the engine runs so this block's
    // measurement starts clean, and the cleared values are published HERE
    // rather than left to the post-process publish: a short-circuited block
    // (zero-length) returns before that publish, and the reset must not sit
    // invisible until real audio arrives. Same publish set as prepareToPlay.
    if (meterResetPending.exchange (false, std::memory_order_acquire))
    {
        engine.resetMeterHolds();
        dbTpMaxHold = samplePeakMaxHold = -144.0f;
        publishSilentMeters();   // superset of the three this needed; see there
    }

    // Build the POD snapshot ONCE per block (ADR-0001/ADR-0011).
    cached.toEngine (snapshot);
    snapshot.oversample      = internalState.oversampleFactor();
    snapshot.osPhase         = internalState.osPhaseMode();
    snapshot.forceMaxOffline = internalState.forceMaxOffline();
    snapshot.nonRealtime     = nonRealtimeFlag.load (std::memory_order_relaxed);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // mono → stereo: duplicate the mono input into the second channel (the
    // clear loop above just zeroed it), exactly as the sibling does. The
    // engine is prepared from getTotalNumOutputChannels() — stereo — so both
    // channels are processed; without this the right channel would master
    // silence. See isBusesLayoutSupported for why mono is accepted at all.
    if (getMainBusNumInputChannels() == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    // A block the engine short-circuited produced no render-tap values:
    // publishing anyway would re-report the previous block's peaks and push a
    // duplicate GR-history entry, breaking the one-entry-per-processed-block
    // property the ring's readers rely on. The engine reports the fact rather
    // than the wrapper re-deriving its early-return condition.
    if (! engine.process (buffer, snapshot))
        return;

    // -- §2.9 metering: publish once per block from the engine's RENDER tap
    // (relaxed atomics — monotonic display data, THREAD_MODEL meter row).
    // NOT from `buffer`: the buffer carries the LISTENING path, which the
    // §2.7 monitor functions alter — metering it made Delta show the
    // difference signal's loudness, Comp the attenuated level, and both
    // permanently biased the session-cumulative integrated LUFS and dBTP
    // hold. The render tap is the programme output (identical to the buffer
    // whenever the monitor functions are off). Same audio thread, right
    // after process(): plain reads, no atomics needed on this side.
    const auto& om = engine.outputLoudness();
    const float blockTpDb = juce::Decibels::gainToDecibels (engine.lastRenderTpMax(), -144.0f);
    dbTpMaxHold = juce::jmax (dbTpMaxHold, blockTpDb);
    // The SAMPLE peak's own hold (ADR-0020). `lastRenderPeak()` already
    // existed and fed the GR ring's waveform; the stats row needs it as a
    // held level too, and it is a max-hold rather than a per-block figure for
    // the same reason the true peak is — the question "did this master ever
    // exceed X?" is a session question.
    samplePeakMaxHold = juce::jmax (samplePeakMaxHold,
                                    juce::Decibels::gainToDecibels (engine.lastRenderPeak(), -144.0f));

    // `integratedLufs()` and `lraLu()` are CACHED inside `LoudnessMeter` and
    // cost a branch here between gating blocks; the histogram walks they hold
    // (~1500 and ~2250 iterations) run at 10 Hz rather than per block. This
    // used to carry a TODO proposing exactly that, on the grounds that the
    // figures only move when a gating block commits: correct, and what made
    // the change bit-identical rather than an approximation. The reason it
    // stopped being optional is the block RATE — at 192 kHz with 32-sample
    // buffers the per-block form was ~22 M iterations/s, the same order as
    // DESIGN §9's whole ≤ 0.5 % metering allocation. See the note above
    // `integratedLufs()` for the invalidation and the thread argument.
    const float lufsI = om.integratedLufs();
    pubLufsM.store (om.momentaryLufs(),  std::memory_order_relaxed);
    pubLufsS.store (om.shortTermLufs(),  std::memory_order_relaxed);
    pubLufsI.store (lufsI,               std::memory_order_relaxed);
    pubDbTpMax.store (dbTpMaxHold,       std::memory_order_relaxed);
    // PLR = session true-peak max − integrated loudness (meaningful only once
    // both exist; 0 until then).
    pubPlr.store (lufsI > anabasis::LoudnessMeter::kSilentLufs + 1.0f
                      ? dbTpMaxHold - lufsI : 0.0f,
                  std::memory_order_relaxed);
    // The stats row (ADR-0020). `integratedUngatedLufs()` and `lraLu()` are
    // published UNCONDITIONALLY beside the gated figure — the Settings choice
    // between BS.1770-1 and -2 and between the two RMS references is resolved
    // by the VIEW, so the audio thread never reads a display preference and
    // switching either one is instant with no audio involvement.
    pubPeakMaxDb.store (samplePeakMaxHold, std::memory_order_relaxed);
    pubRmsDb.store (engine.outputRms().rmsDb(), std::memory_order_relaxed);
    pubLufsIUngated.store (om.integratedUngatedLufs(), std::memory_order_relaxed);
    pubLra.store (om.lraLu(), std::memory_order_relaxed);

    const float grDb = juce::Decibels::gainToDecibels (engine.lastBlockMinGain(), -60.0f);
    pubGrDb.store (grDb, std::memory_order_relaxed);
    // ONE entry per processBlock CALL, which is the ring's documented contract
    // — and the span it covers is the HOST's block, not the prepared one. The
    // engine chunks an oversize block internally while `grMinThisCall` and the
    // render peak accumulate across every chunk, so a host running 4096 with
    // 512 prepared publishes one entry describing 85 ms. Correct for a
    // worst-case display and wrong for a time axis drawn as if entries were
    // evenly spaced: the P5 GR-history renderer needs a time base, not an
    // index count. Recorded here rather than in the ring, which cannot know.
    grHistoryRing.push (grDb, engine.lastRenderPeak());
}

juce::AudioProcessorEditor* AnabasisAudioProcessor::createEditor()
{
    return new AnabasisAudioProcessorEditor (*this);
}

// ---------------------------------------------------------------------------
//  Schema v1 (ADR-0007)
// ---------------------------------------------------------------------------
juce::ValueTree AnabasisAudioProcessor::copyStateWithRaw()
{
    // Anamorph ADR-0013's additive exact-`raw` attribute, stamped on EVERY
    // serialised copy of the parameter tree — the session's ANABASIS child
    // AND the AB slots — so every restore path can be raw-exact. Stamping
    // only the top-level copy left A/B switching value-only: log-taper
    // params drift ulps through the pow/log round trip and discrete Raw*
    // params lose their mid-step values.
    auto params = apvts.copyState();
    for (int i = 0; i < params.getNumChildren(); ++i)
    {
        auto node = params.getChild (i);
        if (node.hasType ("PARAM"))
            if (auto* p = apvts.getParameter (node.getProperty ("id").toString()))
                node.setProperty ("raw", (double) p->getValue(), nullptr);
    }
    return params;
}

void AnabasisAudioProcessor::adoptParamsTree (const juce::ValueTree& paramsWithRaw)
{
    // Strip the `raw` overlay BEFORE replaceState — the live tree never
    // carries it (save→load→save must stay byte-identical) — then re-assert
    // from the unstripped copy, which is what makes the restore raw-exact.
    auto stripped = paramsWithRaw.createCopy();
    for (int i = 0; i < stripped.getNumChildren(); ++i)
        stripped.getChild (i).removeProperty ("raw", nullptr);
    apvts.replaceState (stripped);
    reassertFromRaw (paramsWithRaw);
}

// ADR-0014 ownership, stated once. WHO OWNS THE FROZEN VECTOR: the ENGINE
// does, in `AdaptiveEngine`'s RETAINED trim set — four lock-free scalars plus
// a release-stored flag, which `reset()` deliberately does not clear, so the
// vector outlives the engine's own re-initialisation exactly as `learned` and
// the two reference targets do. The wrapper's `liveFrozenTrims` mirror is NOT
// the owner and never became one; it covers precisely the window in which the
// engine's answer is out of date, which is one condition:
//
//   * a restore staged for this slot is still unapplied — between the
//     block-top consume and the duck's silent bottom (~34 ms, and unbounded if
//     no audio runs at all) the retained vector is the PRE-restore one and the
//     mirror holds the loaded truth.
//
// The second condition is about existence rather than staleness: an instance
// that has never latched anything has generation 0, and the four scalars sit at
// an initial zero indistinguishable from a measured "no trim", so adopting them
// would write zeros over a vector the slot holds.
//
// The THIRD condition is SLOT OWNERSHIP, and it is the one this function was
// missing. `FROZEN_TRIMS` is per-slot state; the retained set is engine-wide and
// carries no slot identity — the engine latches a vector, not "slot A's vector".
// So after an A/B switch into a freeze-ON slot that carries NO child of its own,
// nothing stages a restore (the stage is gated on the mirror being valid), the
// generation pair stays equal, and the next save for the INCOMING slot happily
// serialised the OUTGOING slot's latch as if it owned it — and the next restore
// then injected it. The retained set is a runtime CACHE of the last latch; the
// per-slot child is the persistent record, and a cache may only answer for the
// slot it was filled under. `slotFrozenBase` is the generation at the moment the
// live surface's frozen ownership last changed, so `generation != base` means
// "this engine has latched something since this slot became live", which is
// exactly the question. Equal ⇒ whatever the engine holds belongs to somebody
// else, and the mirror (or nothing at all) is the honest answer.
//
// Relaxed on the base, deliberately: unlike the generation it is compared
// against, it announces no payload — it is a comparand recorded by the same
// thread that replaced the ownership, and the values it gates are already
// ordered by the generation's own acquire.
//
// Round 40 read this rule off the PUBLISHED set instead, which does not
// survive a re-prepare, and rescued the difference by copying the latch into
// the mirror from `prepareToPlay` — a host callback JUCE does not deliver on
// the message thread, writing a `juce::ValueTree` the editor's dirty poll
// reads several times a second. The retained set removes the rescue and the
// race together: the durable copy never leaves the lock-free layer, and this
// function reads it from whichever thread asks. Freeze OFF returns nothing at
// all: MODE invariant 3 gives an unfrozen slot no latch (and §5.4's slew would
// walk away from one on the next audible block).
// The ONLY way `liveFrozenTrims` is written, for the reason `replaceDetachMask`
// is the only way the mask is: replacing the live surface's frozen vector and
// re-basing the slot-ownership comparand are two halves of ONE rule, not a
// courtesy beside it. Three call sites had the first half — `applySlotToLive`
// (A/B, undo, redo), `resetSlotFieldsToDefaults`, and `setStateInformation`'s
// own read of the AB child — and a fourth would have had to remember it.
//
// Re-basing to the CURRENT generation says "nothing the engine holds belongs to
// this surface yet". A restore staged a line later re-establishes ownership the
// moment it is applied (`injectTrims` publishes, which advances the generation
// past the base); audio on the new slot does the same through `finishBlock`.
// Both are the right answer for the same reason: ownership follows the latch.
void AnabasisAudioProcessor::adoptFrozenMirror (juce::ValueTree frozen)
{
    liveFrozenTrims = std::move (frozen);

    // THE ORDER OF THESE TWO IS LOAD-BEARING, and it is not symmetric — read
    // this before "tidying" them, because no test catches a swap (the
    // distinguishing case needs an audio publication to land BETWEEN them, and
    // the headless suite is single-threaded).
    //
    // The two are not one atomic operation and cannot be made one without a
    // lock: a `juce::ValueTree` assignment and an atomic load do not combine.
    // The audio thread can publish in the gap — only with Freeze OFF, since
    // `finishBlock` stops publishing while frozen — so the boundary can be one
    // generation off. WHICH WAY it is off is the whole question:
    //
    //   * Reading AFTER the mirror write (what this does) can only make the
    //     boundary too LATE. A publication that lands in the gap is folded into
    //     the base, i.e. attributed to the OUTGOING slot, and the new slot
    //     withholds a latch it arguably owns until the next publication — ~10 ms
    //     of audio away, and it writes nothing wrong in the meantime.
    //   * Reading BEFORE it would make the boundary too EARLY, and a
    //     publication in the gap would then satisfy `gen != base` for the
    //     INCOMING slot — serialising a vector measured while the outgoing slot
    //     was still live. That is precisely the cross-slot leak round 42
    //     existed to close, re-opened in miniature.
    //
    // So the skew is deliberately biased toward silence rather than toward
    // borrowing another slot's latch, which is the same trade every other
    // clause of this rule makes. The boundary means: "no latch published up to
    // and including this point belongs to this slot."
    const auto ownershipBoundary = engine.adaptiveForWrapper().retainedTrimGeneration();
    slotFrozenBase.store (ownershipBoundary, std::memory_order_relaxed);
}

juce::ValueTree AnabasisAudioProcessor::engineFrozenTrimsIfLive()
{
    if (apvts.getRawParameterValue (pid::freeze)->load() < 0.5f)
        return {};
    const auto& a = engine.adaptiveForWrapper();
    const auto gen = a.retainedTrimGeneration();          // ACQUIRE: gates the four reads below
    if (gen == 0 || gen == slotFrozenBase.load (std::memory_order_relaxed)
        || engine.frozenRestorePending())
        return {};
    juce::ValueTree ft ("FROZEN_TRIMS");
    ft.setProperty ("releaseOctaves", (double) a.retainedTrimRelease(), nullptr);
    ft.setProperty ("stereoLink",     (double) a.retainedTrimLink(), nullptr);
    ft.setProperty ("scHpfHz",        (double) a.retainedTrimHpf(), nullptr);
    ft.setProperty ("dynTiltDb",      (double) a.retainedTrimTilt(), nullptr);
    return ft;
}

juce::ValueTree AnabasisAudioProcessor::saveSlotFromLive()
{
    // The slot serialises the FULL parameter tree, view-tier entries included;
    // the "view state never travels with a slot" rule lives entirely on the
    // apply side (applySlotToLive overwrites those entries from LIVE before
    // adopting). Consequence for later phases: any path that ever adopts a
    // slot tree WITHOUT going through applySlotToLive — an undo stack, the
    // P2 duck-routed swap — silently re-introduces view-tier travel. Route
    // every slot adoption through applySlotToLive, or move the exclusion here
    // first.
    juce::ValueTree slot ("SLOT");
    slot.setProperty ("presetName", livePresetName, nullptr);
    slot.appendChild (copyStateWithRaw(), nullptr);
    if (liveBaseline.isValid())
        slot.appendChild (liveBaseline.createCopy(), nullptr);
    // ADR-0014 capture: with Freeze ON the latched vector IS the published
    // one, so the save reads it live — unless a restore staged for this slot
    // has not been APPLIED yet (a load-then-save with no audio between, or the
    // ~34 ms between the block-top consume and the duck bottom), where the
    // engine's published trims are STALE and the restored copy is the truth:
    // the same mirror rule the ADAPTIVE child follows.
    //
    // The result goes into a LOCAL, never back into `liveFrozenTrims`: the
    // mirror is written by the restore paths only, and a query that rewrote it
    // would destroy a just-loaded vector the moment it ran inside ADR-0014's
    // window. That rule was originally forced by `presetDirty()` reaching here
    // ~3 Hz; the dirty marker now compares `presetShapeFromLive()` and never
    // enters this function, so the remaining callers are the deliberate ones
    // (A/B swap, undo push, `getStateInformation`). Keep it a pure read anyway:
    // the callers that remain are exactly the ones a rewrite would corrupt.
    //
    // FREEZE OFF ⇒ NO CHILD AT ALL. `frozen` used to start from the mirror
    // unconditionally, so a slot that was frozen, loaded, then UN-frozen kept
    // writing the old vector into every later save — a latch serialised by a
    // slot that no longer holds one. Inert for audio (both landing sites stage
    // it only for a freeze-ON adopted surface) but not inert for state: it is
    // the record of a Freeze the user has switched off, and it is a child of
    // the tree `presetDirty()` compares, so its presence flipped the edited
    // mark. §5.4/MODE invariant 3 give a freeze-OFF slot nothing to latch.
    juce::ValueTree frozen;
    if (apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f)
    {
        frozen = liveFrozenTrims;              // the staged-but-unapplied window
        if (const auto live = engineFrozenTrimsIfLive(); live.isValid())
            frozen = live;                     // …the owner, once it is current
    }
    if (frozen.isValid())
        slot.appendChild (frozen.createCopy(), nullptr);
    juce::ValueTree mask ("DETACH_MASK");
    for (const auto& id : liveDetachMask)
    {
        juce::ValueTree m ("PARAM");
        m.setProperty ("id", id, nullptr);
        mask.appendChild (m, nullptr);
    }
    slot.appendChild (mask, nullptr);
    return slot;
}

juce::ValueTree AnabasisAudioProcessor::presetShapeFromLive() const
{
    // THE LIVE STATE PROJECTED ONTO WHAT A PRESET CAN CARRY — the non-excluded
    // parameters at their snapped preset values plus the §5.3 detach mask.
    // "Exactly `PresetManager::savePreset`'s content" is now true BY
    // CONSTRUCTION rather than by value: both run `forEachPresetParameter`, so
    // there is one traversal, one exclusion test and one value rule between
    // them. Round 51 shared the two rules but left the two walks distinct — this
    // one over `getParameters()`, the writer's over `apvts.state`'s PARAM
    // children — which agreed only because APVTS happens to create one tree
    // child per parameter. That is a fact about JUCE, not an invariant of this
    // code, and a parameter registered without a node (or the reverse) would
    // have put content in the file the marker could not see.
    //
    // The dirty marker used to compare SLOT trees, and that was the wrong datum
    // for the question it answers. A slot carries the full parameter surface
    // (view-tier entries included), the exact-`raw` attribute, BASELINE and
    // FROZEN_TRIMS — and a preset file stores none of those. So resizing the
    // editor, switching the Advanced panel, toggling Freeze, or a macro gesture
    // writing a baseline lit "edited" on a preset whose file would have been
    // byte-identical, and re-saving could not honestly clear it: the mark
    // claimed a difference the format cannot express. It also went the other
    // way — a mid-step `raw` move on a discrete parameter marked edited while
    // the snapped value a preset stores had not moved at all. Projecting first
    // makes the marker mean what the top bar says it means.
    //
    // A second consequence, and the reason this is a function rather than a
    // filter inside `presetDirty()`: the editor's ~3 Hz poll no longer runs
    // `saveSlotFromLive()`. That call reached `apvts.copyState()` (the APVTS
    // tree lock — the M1 half of KI-008's inversion), `liveFrozenTrims` and
    // `liveBaseline` (non-thread-safe ValueTrees an off-message-thread
    // `setStateInformation` can write — KI-003). This projection touches
    // neither: the parameter LIST is fixed for the processor's lifetime and
    // each value is an atomic load, so the poll is now lock-free and
    // ValueTree-free. `liveDetachMask` remains — a StringArray with the same
    // KI-003 exposure it always had, unchanged rather than newly introduced.
    juce::ValueTree shape ("PRESET_SHAPE");
    PresetManager::forEachPresetParameter (apvts,
        [&shape] (const juce::String& id, juce::RangedAudioParameter& param)
        {
            juce::ValueTree node ("PARAM");
            node.setProperty ("id", id, nullptr);
            node.setProperty ("value", PresetManager::presetValueOf (param), nullptr);
            shape.appendChild (node, nullptr);
        });

    juce::ValueTree mask ("DETACH_MASK");
    for (const auto& id : liveDetachMask)
    {
        juce::ValueTree m ("PARAM");
        m.setProperty ("id", id, nullptr);
        mask.appendChild (m, nullptr);
    }
    shape.appendChild (mask, nullptr);
    return shape;
}

void AnabasisAudioProcessor::reassertFromRaw (const juce::ValueTree& apvtsTree)
{
    // Anamorph ADR-0013's additive exact-`raw` attribute: the host-session
    // contract is RAW-exact restoration, and discrete Raw* parameters carry
    // mid-step values only through this path.
    for (int i = 0; i < apvtsTree.getNumChildren(); ++i)
    {
        const auto node = apvtsTree.getChild (i);
        if (! node.hasType ("PARAM") || ! node.hasProperty ("raw"))
            continue;
        if (auto* p = apvts.getParameter (node.getProperty ("id").toString()))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) (double) node.getProperty ("raw")));
    }
}

// ADR-0018's `advancedMode` pin, applied to a slot tree at PUSH time. It
// exists for exactly one caller, and the reason is a property no other undo
// entry has.
//
// Every OTHER entry is a `saveSlotFromLive()` taken at the moment of the step
// it records, so its `advancedMode` is by construction the view the user had
// then — which is what makes adopting it on undo correct (`applySlotToLive`
// with `adoptAdvanced = true`, the one path that does). The Copy entry is not
// that: it is `storedSlot`, captured by the last `switchToSlot` and frozen
// since. Toggle ADV after that switch and then Copy, and the destination's
// entry carries the PRE-toggle view mode; undoing the Copy then writes it back
// and the window changes size for a reason the user never took.
//
// ADR-0018 §Consequences states the contract this breaks in as many words:
// "A/B compare behaviour is unchanged: switching slots never resizes the
// editor, and Copy never moves the view." The undo of a Copy is part of Copy's
// behaviour, so the fix belongs here rather than in the ADR. Owner-confirmed
// 2026-08-07: Copy undo keeps the current view mode.
//
// The pin is the same one `applySlotToLive` applies on the Copy and A/B paths
// — value from the live tree, raw from the parameter — deliberately NOT
// generalised into a shared helper: there the rule is "do not adopt what the
// tree carries", here it is "do not store what the tree carries", and the two
// read alike only because they happen to name the same parameter.
juce::ValueTree AnabasisAudioProcessor::slotWithLiveAdvancedMode (const juce::ValueTree& slot)
{
    auto copy = slot.createCopy();
    auto params = copy.getChildWithName ("ANABASIS");
    if (! params.isValid())
        return copy;                     // no surface to pin; the caller's guards cover it
    auto node = params.getChildWithProperty ("id", pid::advancedMode);
    if (! node.isValid())
        return copy;
    if (auto live = apvts.state.getChildWithProperty ("id", pid::advancedMode); live.isValid())
        node.setProperty ("value", live.getProperty ("value"), nullptr);
    if (auto* lp = apvts.getParameter (pid::advancedMode))
        node.setProperty ("raw", (double) lp->getValue(), nullptr);
    return copy;
}

void AnabasisAudioProcessor::applySlotToLive (const juce::ValueTree& slot, bool adoptAdvanced)
{
    const auto params = slot.getChildWithName ("ANABASIS");
    if (params.isValid())
    {
        // View-tier parameters never travel with a slot (the shared
        // predicate): overwrite the incoming copy with the LIVE values —
        // value from the tree, raw from the parameter itself — so both the
        // replaceState and the raw re-assert leave them untouched.
        // `advancedMode` left the view tier in ADR-0018 but stays PINNED on
        // every path except the undo/redo restore (`adoptAdvanced`), so the
        // pin is spelled out here rather than inherited from the predicate.
        auto incoming = params.createCopy();
        for (int i = 0; i < incoming.getNumChildren(); ++i)
        {
            auto node = incoming.getChild (i);
            const auto id = node.getProperty ("id").toString();
            const bool pinned = isViewTierParam (id)
                             || (id == pid::advancedMode && ! adoptAdvanced);
            if (node.hasType ("PARAM") && pinned)
                if (auto live = apvts.state.getChildWithProperty ("id", node.getProperty ("id")); live.isValid())
                {
                    node.setProperty ("value", live.getProperty ("value"), nullptr);
                    if (auto* lp = apvts.getParameter (id))
                        node.setProperty ("raw", (double) lp->getValue(), nullptr);
                }
        }
        adoptParamsTree (incoming);
    }

    livePresetName  = slot.getProperty ("presetName").toString();
    liveBaseline    = slot.getChildWithName ("BASELINE").createCopy();
    adoptFrozenMirror (slot.getChildWithName ("FROZEN_TRIMS").createCopy());
    // ADR-0014 (OQ-013 resolved 2026-08-02, owner-approved — the Hard Stop
    // this banner used to carry is LIFTED): a freeze-ON slot's vector is
    // staged to the engine on ADR-0012's row and lands at the duck's silent
    // bottom — the per-slot Freeze memory restoring, MODE inv 3's last gap.
    // The freeze value is read from the freshly adopted surface, not from the
    // incoming tree: adoptParamsTree already applied the read rules.
    if (liveFrozenTrims.isValid()
        && apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f)
        engine.restoreFrozenTrims (
            (float) (double) liveFrozenTrims.getProperty ("releaseOctaves", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("stereoLink", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("scHpfHz", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("dynTiltDb", 0.0));

    juce::StringArray restored;
    if (const auto mask = slot.getChildWithName ("DETACH_MASK"); mask.isValid())
        for (int i = 0; i < mask.getNumChildren(); ++i)
            restored.add (mask.getChild (i).getProperty ("id").toString());
    replaceDetachMask (restored);
}

// The ONLY way the detach mask is replaced, and the reason it is a function
// rather than two lines repeated: dropping the STAGED inputs is half of
// "replace the mask", not a courtesy beside it.
//
// A gestured managed edit delivered off the message thread sits in
// `pendingDetachBits` for up to one MacroEngine tick (nothing may post from
// that callback — see drainDetachBitsSoon), so the tick AFTER a restore, a
// preset apply, a session load or a reset-to-macro would OR that id into the
// mask this call just installed: a parameter silently detached in a state it
// was never edited in, skipped by the mapper from then on, and serialized with
// the slot. Same for a re-engage that raced the same boundary. A restore is not
// a gesture (§5.3), so a gesture racing it belongs to the state being replaced
// — which is exactly what `MacroEngine::ScopedRestore` does to a pending
// mapping on the way out.
//
// It became a function because the rule was first written at ONE of its five
// sites (`applySlotToLive`) and missed the other four; the mask has no other
// writer now, so a sixth site cannot forget it.
void AnabasisAudioProcessor::replaceDetachMask (const juce::StringArray& newMask)
{
    pendingDetachBits.store (0, std::memory_order_relaxed);
    pendingReengage.store (false, std::memory_order_relaxed);
    liveDetachMask = newMask;
}

void AnabasisAudioProcessor::switchToSlot (int newIndex)
{
    newIndex = anabasis::clampAbSlotIndex (newIndex);
    if (newIndex == activeSlot)
        return;
    // An A/B restore is not a macro gesture (§5.3) — held across the whole
    // swap, not dropped after it, so a drain cannot land between the macro
    // values arriving and the abort.
    const MacroEngine::ScopedRestore guard (*macroEngine);
    // §2.8: BEFORE the swap, so the duck's envelope covers the glide the swap
    // starts — all but its first few ms. The request and the parameter writes
    // are separate stores, and the audio thread reads the parameters (snapshot
    // build) before the flag (block top), so a block can adopt the new values
    // and start the out-leg together: the first ~6 ms of the glide then plays
    // at decreasing but non-zero gain. Still band-limited, never a step.
    // Note the two halves of a swap therefore land at different times: the
    // smoothed parameters glide from that first block, while the discrete
    // rewires the same swap carries (eqPosition, colourModel, OS factor) wait
    // for the silent bottom, which is the whole point of the duck. Only the
    // smoothed half is exposed, and only for the out-leg's first samples.
    engine.requestForcedDuck();
    auto newlyStored = saveSlotFromLive();
    applySlotToLive (storedSlot);
    storedSlot = std::move (newlyStored);
    // The dirty datum swaps WITH the slot: `livePresetName` is per-slot, so a
    // single engine-wide baseline described the wrong slot from here on — after
    // applying a preset in B, switching back to A marked A's name against B's
    // baseline.
    std::swap (presetBaseline, storedPresetBaseline);
    // The in-flight §7 gesture snapshot does NOT swap — it is dropped. A drag
    // open across an A/B switch had its pre-state captured from the OLD slot,
    // and the eventual gesture-end compares it against the NEW slot's live
    // values: the difference is the slot change itself, so the end pushed a
    // step onto `undoStacks[activeSlot]` — the new slot's stack — describing a
    // state the new slot never held. Undo would then "restore" the other
    // slot's values. Per-slot history means a pre-state belongs to the slot it
    // was taken in, and once that slot is no longer active there is nothing it
    // can correctly restore, so both halves go: the open-drag bits (an END
    // that matches nothing pushes nothing — the documented degradation) and
    // the snapshot itself. `managedGestureBits` is deliberately NOT cleared:
    // it decides §5.3 DETACHMENT, and a drag continuing after the switch is
    // now editing the new slot's value, which should detach in the new slot.
    openGestureBits.store (0, std::memory_order_relaxed);
    gesturePreState = {};
    activeSlot = newIndex;
}

bool AnabasisAudioProcessor::applyFactoryPreset (int index)
{
    int count = 0;
    const auto* table = PresetManager::factoryPresets (count);
    if (index < 0 || index >= count)
        return false;                          // validated BEFORE the undo bracket
    pushUndoStep (saveSlotFromLive());

    juce::StringArray mask;
    {
        const MacroEngine::ScopedRestore guard (*macroEngine);
        engine.requestForcedDuck();            // §2.8: a preset is a bulk swap

        // Unreachable given the validation above — and deliberately still
        // checked, because the two guards would only diverge silently if one
        // moved. The early return leaves the pushed undo step and the requested
        // duck standing, exactly as `applyPresetFile`'s does and for the same
        // reason: a failed apply may already have written part of the defaults
        // pass, so those values must land under the duck, and the pre-state is
        // precisely what the undo step should restore. Spending a step and a
        // dip on a partial apply is the correct trade; spending them on a
        // rejected INDEX is not, which is why the index is validated before
        // the bracket opens rather than here.
        if (! presetManager->applyFactoryPreset (index, mask))
            return false;
        replaceDetachMask (mask);
        liveBaseline   = {};                   // defaults-based: no macro baseline survives
        livePresetName = table[index].name;
    }   // the guard drops here, DELIBERATELY before the mapping below

    // A FACTORY preset is not a file preset, and this is where they part.
    // A file carries every parameter, managed ones included, so the restore
    // guard is exactly right there: the mapping must not overwrite what the
    // file landed. A factory table is defaults + a handful of intents, and
    // PresetManager's contract is that it "expresses itself through the MACROS
    // plus non-managed parameters wherever possible" — so after the defaults
    // pass the nine §5.5 managed parameters sit at their DEFAULTS and only the
    // macro positions describe the preset. The guard swallowed the mapping
    // those positions armed (its destructor aborts whatever is pending) and
    // nothing re-ran it, so "EDM Club" moved `loudness` to 80 and left the
    // compressor, clipper, limiter and EQ at M(0,0,0): the preset was
    // inaudible. Run the mapping once, here, exactly as `resetToMacro()` does
    // — after the values have landed, outside the guard, and BEFORE the
    // baseline below, or the preset would read as dirty the moment it loaded.
    relandMacroCurve();                        // the mask-replaced invariant lives there

    presetBaseline = presetShapeFromLive();    // dirty marker datum
    return true;
}

bool AnabasisAudioProcessor::applyPresetFile (const juce::File& file)
{
    // §7 preset bracketing: parse BEFORE the bracket opens — an unreadable
    // file must not cost the user an undo step. A partial apply after a
    // successful parse still pushes: the pre-state is exactly what undo
    // should restore in that case. The readability test is PresetManager's own
    // (root tag included), so this gate and the apply cannot disagree — an
    // is-it-XML gate let a foreign root through and charged an undo step for a
    // guaranteed no-op.
    // ONE parse, used by both halves. The gate used to parse and throw the
    // document away, leaving `applyPreset` to read the file again — so a file
    // rewritten between the two (the ring walks this path on every ‹/› press)
    // could pass the gate and then apply different content. The gate's meaning
    // is unchanged: `parsePresetFile` is still the single readability answer,
    // root tag included.
    const auto parsed = file.existsAsFile() ? PresetManager::parsePresetFile (file) : nullptr;
    if (parsed == nullptr)
        return false;
    pushUndoStep (saveSlotFromLive());

    const MacroEngine::ScopedRestore guard (*macroEngine);   // §5.3, as above
    // DELIBERATELY before the apply, and NOT undone on failure: a failed
    // applyPreset may still have written some parameters (a partial apply),
    // and those must land under the duck. Moving this after the success check
    // to save a ~34 ms dip on the failure path would reopen the unducked
    // bulk-swap hole INC-001 records.
    engine.requestForcedDuck();                               // §2.8, as above

    juce::StringArray mask;
    if (! presetManager->applyPreset (*parsed, mask))
        return false;                     // the guard still runs: a partial
                                          // apply arms the listeners too
    replaceDetachMask (mask);
    // SYMMETRIC WITH THE FACTORY PATH, and for the identical reason: a preset
    // file cannot carry BASELINE. The two apply paths are the same conceptual
    // operation — replace the whole parameter surface from a stored patch — and
    // the factory one has always dropped the macro baseline, while this one
    // left the PREVIOUS state's baseline standing beside a surface it no longer
    // describes. That stale vector then travelled: into the SLOT tree, so A/B
    // and the session save recorded a §5.5 baseline captured against parameter
    // values the slot no longer holds. Undo is unaffected — the pre-state
    // pushed above still carries the old baseline, so undoing the apply
    // restores it with everything else.
    liveBaseline   = {};
    livePresetName = file.getFileNameWithoutExtension();
    presetBaseline = presetShapeFromLive();    // dirty marker datum
    return true;
}

void AnabasisAudioProcessor::resetSlotFieldsToDefaults()
{
    // The §4.4 read rule "missing fields are taken at their defaults" applies
    // to the slot fields too: a valid root WITHOUT an AB child must not leave
    // the previous session's mask/trims/name/slot behind, or the next save
    // serialises a chimera of two sessions.
    activeSlot = 0;
    livePresetName = "Default";   // the field's default IS the Default preset's name
    liveBaseline    = juce::ValueTree();
    adoptFrozenMirror ({});
    replaceDetachMask ({});
    storedSlot = defaultSlot.createCopy();
    // The dirty datum is the one slot field that is NOT serialized, so a load
    // cannot restore it — it must be dropped with the rest. Kept, it described
    // a preset applied in the PREVIOUS session and the freshly loaded name
    // rendered as edited (or, as wrongly, as clean). Absent, `presetDirty()`
    // returns false until the next apply or save, which is the honest answer:
    // a session records which preset a slot holds, never whether it had been
    // edited since.
    presetBaseline       = juce::ValueTree();
    storedPresetBaseline = juce::ValueTree();
}

void AnabasisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("AnabasisRoot");
    root.setProperty ("schemaVersion", kSchemaVersion, nullptr);

    // ANABASIS with the additive exact-`raw` attribute per PARAM.
    root.appendChild (copyStateWithRaw(), nullptr);
    root.appendChild (internalState.state().createCopy(), nullptr);

    juce::ValueTree ab ("AB");
    ab.setProperty ("active", activeSlot, nullptr);   // ADR-0007's field name
    ab.appendChild (activeSlot == 0 ? saveSlotFromLive() : storedSlot.createCopy(), nullptr);
    ab.appendChild (activeSlot == 0 ? storedSlot.createCopy() : saveSlotFromLive(), nullptr);
    root.appendChild (ab, nullptr);

    // ADAPTIVE: "absent = never learned" is the §4.4 discriminator, so the
    // child is written ONLY once Learn has committed targets. The values are
    // audio-thread-written atomics read here on the message thread — stable
    // after the commit, the same capture pattern as the frozen-trim latch.
    //
    // While a restore is still STAGED (loaded, not yet consumed by a block
    // top) the engine's answer is one session out of date, so the staged
    // record is authoritative instead: a host that loads a project and
    // re-saves it without running audio must not lose — or resurrect — a
    // learned reference. Once consumed the two agree.
    //
    // Residual window, stated exactly rather than claimed away: the consumer
    // clears the flag with `exchange` and adopts a few instructions LATER, so
    // a save landing between the two reads `false` here and falls back to the
    // engine's pre-adoption values. Cost is one save's worth of learned
    // references; closing it would need the flag cleared after adoption, which
    // trades this window for a lost-update one (a stage arriving between adopt
    // and clear would be erased). ADR-0012 §Known limits records the choice.
    const bool  restoreStaged = engine.adaptiveRestorePending();
    const auto& ad            = engine.adaptiveForWrapper();
    const bool  learnedNow    = restoreStaged
                                  ? stagedAdaptiveLearned.load (std::memory_order_relaxed)
                                  : ad.hasLearned();
    if (learnedNow)
    {
        juce::ValueTree adaptive ("ADAPTIVE");
        adaptive.setProperty ("refOnsetRate",
                              (double) (restoreStaged
                                            ? stagedRefOnset.load (std::memory_order_relaxed)
                                            : ad.publishedRefOnset()),
                              nullptr);
        adaptive.setProperty ("refTiltDb",
                              (double) (restoreStaged
                                            ? stagedRefTilt.load (std::memory_order_relaxed)
                                            : ad.publishedRefTilt()),
                              nullptr);
        root.appendChild (adaptive, nullptr);
    }

    if (const auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AnabasisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;                                        // corrupt input: keep current state
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("AnabasisRoot"))
        return;                                        // foreign input: keep current state
    // schemaVersion 1 is the only generation; missing → treated as 1
    // (structural-tolerance read rules, §4.4).

    // §5.3 again, and this is the path that needs the SCOPE rather than a
    // trailing abort: VST3 does not promise `setStateInformation` arrives on
    // the message thread, so the 30 ms drain timer can fire in the middle of
    // the restore below. See KNOWN_ISSUES KI-003 for what this does and does
    // not cover.
    const MacroEngine::ScopedRestore guard (*macroEngine);
    engine.requestForcedDuck();   // §2.8: a session load is the biggest bulk swap of all
    // §7: undo stacks are session-local and never serialized — a load starts
    // a fresh history for both slots. It is announced rather than performed:
    // this function is not promised on the message thread, and the stacks are
    // plain containers the editor reads at display rate and pops from. The
    // counter is the whole message; `syncHistory()` does the clearing on the
    // one thread allowed to touch them. See `historyEpoch`.
    historyEpoch.fetch_add (1u, std::memory_order_relaxed);
    openGestureBits.store (0, std::memory_order_relaxed);
    // …and the THIRD member of the gesture-state family, which the load did
    // not reset. `managedGestureBits` is set and cleared on ANY thread (an
    // off-thread drag detaches), so a BEGIN delivered off-thread with the
    // session replaced before its matching END left the bit standing — and the
    // next ungestured write to that managed parameter would then satisfy the
    // "gesture-bracketed" half of the §5.3 discriminator. The other two
    // conditions keep the exposure narrow (the mapper's own writes are covered
    // by `isApplyingMacro`/`isRestoring`), but a load starts a fresh session
    // and no gesture from the previous one may outlive it: the three members
    // are one family and now reset together.
    managedGestureBits.store (0, std::memory_order_relaxed);
    // The P5 decision THREAD_MODEL left open, taken: a state load CLEARS the
    // session-cumulative meter holds. The integrated LUFS and the dBTP hold
    // describe the programme measured so far, and after a load the programme
    // is a different session's — keeping the old maximum "will read as a bug
    // the first time a meter is visible" (the reviewer's words, and correct).
    // Staged through the same momentary-request row as the GUI button, so the
    // clear lands at a block top like every other cross-thread command. The
    // display publish that used to sit on the next line has moved INTO
    // `requestMeterReset` — a project is ordinarily opened with the transport
    // stopped and no block then runs at all, and that is just as true of the
    // meter panel's own click, which had only the flag. See the request's
    // declaration for why the pairing belongs there rather than here.
    requestMeterReset();

    // Same read rule for the parameter tree: a valid root that omits ANABASIS
    // means "defaults", not "keep whatever is live".
    if (const auto params = root.getChildWithName ("ANABASIS"); params.isValid())
        adoptParamsTree (params);
    else
        adoptParamsTree (defaultSlot.getChildWithName ("ANABASIS"));
    internalState.replaceFrom (root.getChildWithName ("ANABASIS_INTERNAL"));

    // Slot fields FIRST go to defaults (the missing-fields read rule), then
    // whatever the AB child actually carries overlays them.
    resetSlotFieldsToDefaults();
    if (const auto ab = root.getChildWithName ("AB"); ab.isValid())
    {
        // Collect SLOT children BY TYPE, never by raw position: the
        // tolerance rules admit unknown children, and indexing ab.getChild(i)
        // directly would let a tolerated foreign child shift both slots.
        juce::Array<juce::ValueTree> slots;
        for (int i = 0; i < ab.getNumChildren(); ++i)
            if (ab.getChild (i).hasType ("SLOT"))
                slots.add (ab.getChild (i));

        activeSlot = anabasis::clampAbSlotIndex ((int) ab.getProperty ("active", 0));
        const auto live   = activeSlot     < slots.size() ? slots[activeSlot]     : juce::ValueTree();
        const auto stored = 1 - activeSlot < slots.size() ? slots[1 - activeSlot] : juce::ValueTree();
        if (stored.isValid())
            storedSlot = stored.createCopy();
        if (live.isValid())
        {
            // The ANABASIS child above is already the live surface; take the
            // slot's non-parameter fields (name/baseline/trims/mask) from AB.
            livePresetName  = live.getProperty ("presetName").toString();
            liveBaseline    = live.getChildWithName ("BASELINE").createCopy();
            adoptFrozenMirror (live.getChildWithName ("FROZEN_TRIMS").createCopy());
            juce::StringArray loadedMask;
            if (const auto mask = live.getChildWithName ("DETACH_MASK"); mask.isValid())
                for (int i = 0; i < mask.getNumChildren(); ++i)
                    loadedMask.add (mask.getChild (i).getProperty ("id").toString());
            replaceDetachMask (loadedMask);
        }
    }

    // ADR-0014: a freeze-ON session restores its frozen vector too — the same
    // stage applySlotToLive performs, against the same duck this load already
    // requested. The freeze value is read from the adopted surface (the
    // ANABASIS child above), never the raw tree. liveFrozenTrims doubles as
    // the mirror: while the stage is unconsumed, saveSlotFromLive serialises
    // it instead of the engine's stale published trims.
    if (liveFrozenTrims.isValid()
        && apvts.getRawParameterValue (pid::freeze)->load() >= 0.5f)
        engine.restoreFrozenTrims (
            (float) (double) liveFrozenTrims.getProperty ("releaseOctaves", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("stereoLink", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("scHpfHz", 0.0),
            (float) (double) liveFrozenTrims.getProperty ("dynTiltDb", 0.0));

    // ADAPTIVE read rules: present → restore the learned targets through the
    // mirror pattern (consumed at the next block top); absent → never
    // learned, defaults (§4.4's discriminator).
    // A missing FIELD inside a present child takes its default (§4.4), which
    // here is the factory neutral reference — not var()'s 0.0, which would
    // have the trims chase a reference no programme material can match.
    // The staged record is mirrored here (message thread) so getStateInformation
    // can answer correctly before the next block top consumes it.
    //
    // INVARIANT: the mirror store and the engine stage must stay PAIRED. This
    // is the only site that stages an adaptive record today; a future one (a
    // preset carrying adaptive data, say)
    // that calls restoreLearnedTargets/restoreNeverLearned without updating
    // the mirror would raise `adaptivePending` while the mirror still held the
    // previous record — and getStateInformation, which prefers the mirror
    // exactly while that flag is up, would serialize the stale one. Route any
    // new stager through here, or pair the two stores in a helper first.
    if (const auto adaptive = root.getChildWithName ("ADAPTIVE"); adaptive.isValid())
    {
        const auto onset = (float) (double) adaptive.getProperty (
                               "refOnsetRate", anabasis::AdaptiveEngine::kDefaultRefOnset);
        const auto tilt  = (float) (double) adaptive.getProperty (
                               "refTiltDb", anabasis::AdaptiveEngine::kDefaultRefTilt);
        stagedRefOnset.store (onset, std::memory_order_relaxed);
        stagedRefTilt.store (tilt, std::memory_order_relaxed);
        stagedAdaptiveLearned.store (true, std::memory_order_relaxed);
        engine.restoreLearnedTargets (onset, tilt);
    }
    else
    {
        stagedRefOnset.store (anabasis::AdaptiveEngine::kDefaultRefOnset,
                              std::memory_order_relaxed);
        stagedRefTilt.store (anabasis::AdaptiveEngine::kDefaultRefTilt,
                             std::memory_order_relaxed);
        stagedAdaptiveLearned.store (false, std::memory_order_relaxed);
        engine.restoreNeverLearned();
    }

    // Deliberately the SECOND recompute of this load: replaceFrom's batch
    // already fired one (that is the level testLatencyNotifyIsBatchedAcrossARead
    // pins). This one is belt-and-braces for the rest of the restore body, and
    // costs nothing — setLatencySamples no-ops when the figure is unchanged,
    // which it is, so the host sees at most one PDC change per load either way.
    updateLatency();
}

// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnabasisAudioProcessor();
}
