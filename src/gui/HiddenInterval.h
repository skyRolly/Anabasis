#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abgui
{

// ============================================================================
//  HiddenInterval — the seconds a visibility-gated view spent NOT ticking.
//
//  The graph well shows one of two views (`GrHistoryView`, `SpectrumView`) and
//  swaps them by visibility alone. Each stops its `FrameClock` when it is
//  hidden, which is the right thing to do — a view nobody can see must not
//  paint — and it has one consequence both views share:
//
//      WHILE A VIEW IS HIDDEN NOTHING PUBLISHES, BUT TIME STILL PASSES.
//
//  Both views carry display state that is a function of ELAPSED TIME rather
//  than of the data alone — the GR view's smoothed head ramps at the nominal
//  entry rate between arrivals, the spectrum's per-bin EMA decays with a
//  ~120 ms time constant — and both derive their step from the `dt` the frame
//  clock hands them, "the REAL elapsed seconds since the tick it last ran"
//  (`FrameClock`). A stopped clock does not stop the seconds; it only stops
//  the ticks that would have counted them. So the state a view resumes with
//  is the state it had when it was hidden, and it goes on evolving from
//  there — the ramp replays the remainder of a step that had already expired,
//  the EMA holds levels that had already decayed away. The frame clock cannot
//  supply the missing seconds and deliberately does not try: `start()` resets
//  its pacing so the first tick after a restart carries a neutral 1/60 s
//  rather than the whole gap (its own comment says so), which is correct for
//  PACING and useless as a measure of the gap.
//
//  This is the missing measurement, in one place because the rule is one rule:
//  stamp the wall clock when the view goes away, and when it comes back hand
//  the difference to that view's own tick BEFORE the clock is restarted. What
//  the seconds then MEAN is each view's own business — the two state models
//  are not analogous and are not fixed by analogy — but neither view can apply
//  its model without them.
//
//  Wall clock, not a frame count: the gap has to be measurable across an
//  interval in which this view ran no frames at all, and the producer's own
//  clock (ring entries, scope frames) stops exactly when the transport does,
//  which is the case that most needs the answer.
//
//  `juce::Time::getMillisecondCounterHiRes` is the caller's to read — the same
//  one the editor's own animation timing uses — so this type stays a pure
//  arithmetic object that a test can drive with times of its choosing.
// ============================================================================
class HiddenInterval
{
public:
    // Nothing has been hidden yet: the first reveal is not a resume, and its
    // gap is 0 rather than "since the epoch".
    static constexpr double kNotStopped = -1.0;

    // The view went away — record when. Called from `visibilityChanged`, on
    // the message thread, beside `FrameClock::stop()`.
    void stopped (double nowMs) noexcept { stampMs = nowMs; }

    // The view is back: the seconds its clock was stopped for, 0 if it was
    // never stopped. CONSUMES the stamp, so a second reveal with no hide in
    // between (which `visibilityChanged` cannot produce — JUCE sends it only
    // on a change — but a direct caller could) reads 0 rather than a gap that
    // has already been accounted for.
    double resumedSeconds (double nowMs) noexcept
    {
        const double gap = gapSeconds (stampMs, nowMs);
        stampMs = kNotStopped;
        return gap;
    }

    // Pure, and public, because this is the whole of the arithmetic and it has
    // two cases worth pinning by test rather than by reading: never stopped
    // reads 0, and a clock that appears to run BACKWARDS (no monotonicity is
    // promised by anything here) reads 0 too rather than handing a view a
    // negative dt, which would drive the GR ramp backwards — the one direction
    // its own invariant forbids.
    static double gapSeconds (double stampMs, double nowMs) noexcept
    {
        if (stampMs < 0.0)
            return 0.0;
        return juce::jmax (0.0, (nowMs - stampMs) * 0.001);
    }

private:
    double stampMs = kNotStopped;
};

} // namespace abgui
