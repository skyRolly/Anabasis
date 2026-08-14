# ADR-0027 — A look-and-feel override may read one editor counter from the painting thread, through an atomic

> **⊕ NOT RATIFIED — THE ARCHITECTURE REVIEW GATE IS OPEN.** `ARCHITECTURE_REVIEW_GATE.md` lists
> "**Thread Model change** — new thread, new cross-thread path, new atomic ordering
> (`THREAD_MODEL.md`)" among the changes that "must NOT be auto-merged even if CI, the self-tests,
> and pluginval all pass"; `CLAUDE.md` repeats "threading-model change" in its hard-stop list; and
> `THREADING_POLICY.md`'s allowed-paths table closes with "Any path not in this table is a new
> cross-thread path → Architecture Review Gate." This is such a path, and 0.1.4 introduced it
> **without flagging it**. Found by review, 2026-08-13.
>
> **The self-indicting part, recorded because it is the useful half of the lesson.** The paragraph
> added to `THREAD_MODEL.md` in the round that made the read atomic says, in its own words, that
> this is "the first GUI-side cross-thread read that is NOT a stateless `const` peek". That
> sentence IS the gate trigger, written out by the change that should have stopped for it, in the
> same commit — while the PR description continued to assert "no threading change". A rule can be
> quoted accurately and still not be applied.

**Status:** Proposed — 2026-08-13. **Not** covered by the standing blanket approval for the
post-v0.1.0 rounds. `DOCUMENTATION_LIFECYCLE_POLICY.md`'s trigger row for a cross-thread path
requires `THREAD_MODEL.md` **and** `THREADING_POLICY.md` **and** an ADR; only the first was done,
which is what this record and the accompanying table row repair.

## Context

`AnabasisLookAndFeel::drawResizableFrame` has to tell its two JUCE callers apart: a parented pop-up
menu, whose doubled edge it suppresses, and a `ResizableBorderComponent`, whose frame it must draw.
The shape test (a uniform border equal to `getPopupMenuBorderSize()`) is not sufficient on its own —
a resizable component with a 3 px uniform drag zone matches it — so the override also asks the
editor a question: *is a menu parented to you on screen right now?* The editor answers through
`std::function<bool()> isPopupMenuOnScreen`.

**That question is asked from PAINT.** `drawResizableFrame` is reached from
`PopupMenu::MenuWindow::paintOverChildren`, and `THREAD_MODEL.md` §"Which context paints" states the
rule for this tree: when the OpenGL context is attached — macOS and Windows — JUCE paints components
on the GL render thread. The editor's answer is computed from `presetMenusOpen`, which is written on
the MESSAGE thread in three places (`showPresetMenu`, the menu's completion callback, and the tick's
ghost heal).

Two facts about when this arrived, because they are not the same:

- **The PATH is older than the atomic.** `isPopupMenuOnScreen` was wired earlier in the 0.1.4 branch,
  reading a plain `bool shieldRaised`. The cross-thread read existed from that commit; nothing
  flagged it then either.
- **The atomic was the FIX, not the change.** A later round narrowed the predicate to
  `presetMenusOpen` and made it `std::atomic<int>` after review pointed out the race. That removed
  the undefined behaviour and left the path — which is the thing the gate is about — in place.

## Decision

1. **One editor counter may be read from the painting thread**, through `std::atomic<int>`, for the
   sole purpose of letting a look-and-feel override identify its caller.
2. **`memory_order_relaxed` on every access.** The value guards nothing but itself: it orders no
   other memory, publishes no payload, and is not a handshake. A frame that reads a one-tick-stale
   count draws the border it would have drawn a frame earlier.
3. **A hook the paint path invokes is torn down AFTER the painting thread is gone.** The editor's
   destructor calls `glContext.detach()` — which joins the render thread — before assigning
   `nullptr` to `isPopupMenuOnScreen` and `onPopupMenuWindowCreated`. Mutating a `std::function` a
   live render thread may be inside is a race on the callable, independent of the atomicity of what
   it reads.
4. **This row is not a licence to widen.** It permits ONE scalar, relaxed, read-only from the
   painting side, carrying no payload and no ordering. Anything with a payload, anything the paint
   path WRITES, or anything requiring the painting thread to see two values consistently is a new
   path again and returns to this gate.

## Why this shape rather than the alternatives

- **Keep it a plain `int`.** Rejected: it is a data race by the letter of the memory model and
  sanitizer-reportable on the two platforms where the context attaches. That the value is a small
  int and the consequence a mis-drawn border does not make the read defined.
- **Take a lock.** Rejected: locking on the paint path for one integer is worse than the problem,
  and `REALTIME_AUDIO_POLICY`'s instinct against locks in hot paths applies in spirit here.
- **Post the answer to the look-and-feel instead of pulling it.** A `setPopupMenuOnScreen(bool)`
  called from the message thread would move the atomic rather than remove it — the look-and-feel
  member would still be read from paint. Same path, one more moving part.
- **Drop the second test and rely on the shape alone.** Rejected on its own merits earlier in this
  round: a `ResizableBorderComponent` with a 3 px uniform border would silently lose the frame the
  override exists to protect.

## Consequences

- `THREADING_POLICY.md`'s allowed-paths table gains a **Message → Painting** row, marked pending
  this ADR's ratification, so the table stays the authority it claims to be.
- `THREAD_MODEL.md` already carries the mechanism and the teardown-ordering rule; this ADR is the
  decision the policy row and that description both point at.
- **Untestable by the suites, deliberately stated.** No headless test can open a modal menu or run a
  GL render thread, so nothing here is pinned by `AnabasisStateTests`. Under **ADR-0025** that makes
  this a class-1 exception: the mechanism is defended by argument from the framework's own source
  and by the register in `DEPENDENCY_POLICY.md` rule 7, not by a test. What is consequently
  unprotected: a future edit that adds a second painting-thread read, or that reintroduces a
  non-atomic one, fails no gate.
- If the owner declines this, the revert is the whole discriminator: `isPopupMenuOnScreen` is
  removed, `drawResizableFrame` falls back to the shape test alone, and
  `testTheResizableFrameOverrideDiscriminatesItsCallers` loses the half that pins the state test.
  The doubled pop-up edge returns, or the resizable frame does — one or the other, depending on
  which way the shape test is then tuned.

## Verification

`testTheResizableFrameOverrideDiscriminatesItsCallers` pins both halves of the discriminator on the
message thread; the CROSS-THREAD property is not verifiable here (see Consequences). The atomicity
is a compile-time property of the member's type, and the teardown ordering is readable at the one
site that performs it.

## Related code
- `src/gui/PluginEditor.h:330` (the atomic and the reasoning at its declaration)
- `src/gui/PluginEditor.cpp:736` (the wiring, and why it reads `presetMenusOpen` not `shieldRaised`)
- `src/gui/LookAndFeel.h:203-204` (the hook and the override's two-part caller test)
- `src/gui/PluginEditor.cpp:1090` (detach before the hooks are cleared)

Evidence [Verified]:
- Source: `src/gui/PluginEditor.h:330`, `src/gui/PluginEditor.cpp:736`
- Test:   `testTheResizableFrameOverrideDiscriminatesItsCallers` (message-thread half only)
