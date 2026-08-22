// ============================================================================
//  realtime_effects.cpp — COMPILE-TIME realtime enforcement for the first-party
//  leaf layer, via Clang's `-Wfunction-effects`.
//
//  WHY THIS EXISTS BESIDE THE OTHER TWO TIERS (ADR-0029). RealtimeSanitizer and
//  `tests/AllocationGuard.h` are RUNTIME tools: they see what the suite
//  executes. `scripts/check-realtime.py` is a TEXT scan: it sees forbidden
//  constructs written literally inside an audio-path body. Neither follows a
//  CALL out of the bound function into a helper that is not itself bound:
//
//      void helper (int n) { sink.resize (n); }          // allocates
//      void leaf (int n) noexcept [[clang::nonblocking]]
//      { helper (n); }                                   // <- nothing above sees this
//
//  `-Wfunction-effects` does, because it verifies the effect THROUGH THE CALL
//  GRAPH. That is the gap this file closes, and it is the whole reason for it.
//
//  WHY IT IS A SEPARATE TRANSLATION UNIT AND NOT A BUILD-WIDE FLAG. Clang can
//  only infer a callee's effects from a VISIBLE DEFINITION, and JUCE 9.0.1
//  carries none of its own (measured: zero occurrences of `clang::nonblocking`,
//  `clang::nonallocating` or `__rtsan` in the pinned checkout). So over any
//  translation unit that calls into JUCE the flag warns about correct code, and
//  ADR-0029 records the decision not to enable it build-wide.
//
//  What IS clean is the layer below JUCE. This engine has a genuinely JUCE-free
//  leaf layer -- `CeilingClamp.h`, `ScopeBuffer.h`, `Latency.h` and
//  `EngineParameters.h`, four headers that include no JUCE module at all -- and
//  those bodies are pure arithmetic over pre-sized state. Measured on the pinned
//  Clang 22.1.8: the driver below compiles with ZERO `-Wfunction-effects`
//  diagnostics, while the `ANABASIS_EFFECTS_CANARY` block fails with "function
//  with 'nonblocking' attribute must not call non-'nonblocking' function".
//  Clean signal, and it fires -- which is the bar a gate has to pass.
//
//  SCOPE, deliberately narrow: the JUCE-free first-party leaves only. Adding a
//  header that reaches JUCE reintroduces the noise above, so the include list
//  below IS the contract. The modules that DO call JUCE -- the engine, the
//  oversampled stages, the meters -- stay covered by the two runtime tiers and
//  by the static lint.
//
//  It is compiled `-fsyntax-only` by the `realtime` job: no codegen, no link, no
//  run, measured at about a second.
//
//  IT IS COMPILED TWICE, AND THE SECOND COMPILE IS THE LIVENESS PROOF
//  (`TESTING_POLICY.md` rule 5). A clean compile is this gate's entire output,
//  and it is also exactly what a DEAD gate prints: Clang treats an unrecognised
//  `-Werror=<name>` as a mere `-Wunknown-warning-option` WARNING, so the day
//  `function-effects` is renamed or dropped the step would keep exiting 0 while
//  checking nothing. `-Werror=unknown-warning-option` on both compiles makes
//  that case fail by NAME on the first one; the second compile, with
//  `-DANABASIS_EFFECTS_CANARY`, catches the case the option name cannot -- an
//  option still accepted but no longer implemented.
//
//  Seeding the violation into THIS file rather than a separate canary TU is
//  deliberate: it proves the diagnostic on the exact translation unit, include
//  set and flags the gate actually uses, and leaves nothing to drift out of step
//  with them.
// ============================================================================

#include "CeilingClamp.h"
#include "ScopeBuffer.h"
#include "Latency.h"
#include "RealtimeAnnotations.h"

#if defined(ANABASIS_EFFECTS_CANARY)
  #include <vector>
#endif

namespace
{
#if defined(ANABASIS_EFFECTS_CANARY)
    // THE SEEDED VIOLATION, compiled only for the liveness proof. Not annotated,
    // and it grows a `std::vector` -- which is how this project's DSP modules
    // actually size their state (`LookaheadLimiter::prepare`, `RmsMeter::prepare`)
    // -- so a `nonblocking` caller of it is exactly the defect
    // `-Wfunction-effects` exists to report. The diagnostic comes from the CALL
    // GRAPH, which is this tier's whole reason to exist: the error is reported at
    // the CALL below and Clang's notes walk down from there.
    std::vector<float> canarySink;

    void canaryAllocatingHelper (int n) { canarySink.resize ((std::size_t) n); }
#endif

    // The driver is annotated, so every leaf routine it calls must be inferable
    // as nonblocking. Clang walks the call graph from here.
    //
    // WHAT MAKES THE CANARY FAIL IS THE EFFECT, NOT A MISSING ANNOTATION, and
    // this file demonstrates the difference: every call below is to an
    // UNANNOTATED, header-defined function, and the compile that must stay clean
    // contains all of them. Clang infers a callee's effects wherever its
    // definition is visible, so an unannotated leaf whose body is arithmetic
    // diagnoses nothing. The helper that fails is the one whose body blocks.
    void leafAudioPath (float* l, float* r, int n,
                        const anabasis::CeilingClamp& clamp,
                        anabasis::ScopeBuffer& scope,
                        const anabasis::EngineParameters& params,
                        double sampleRate) noexcept ANABASIS_NONBLOCKING
    {
        // §2.6 ceiling backstop, per sample, exactly as the engine calls it.
        const float ceilingLinear = 0.98855f;      // -0.1 dBTP, the default
        for (int i = 0; i < n; ++i)
        {
            l[i] = clamp.processSample (l[i], ceilingLinear);
            r[i] = clamp.processSample (r[i], ceilingLinear);
        }

        // The latency arithmetic the engine runs per block to decide whether the
        // reported PDC still matches the configuration (ADR-0004).
        const int lat = anabasis::predictLatencySamples (params, sampleRate);
        if (lat > 0)
            l[0] = l[0] + 0.0f * (float) lat;      // consume it; no branch removed

        // §2.9 spectrum capture: the SPSC ring's producer side.
        scope.pushBlock (l, r, n);

#if defined(ANABASIS_EFFECTS_CANARY)
        canaryAllocatingHelper (n);   // the gate must report this, or it is dead
#endif
    }
}

// Never linked or run; the compile IS the check.
int main() { return 0; }
