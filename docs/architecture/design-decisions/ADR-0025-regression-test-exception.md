# ADR-0025 — A narrow, disclosure-bound exception to TESTING_POLICY rule 1

**Status:** Accepted — 2026-08-13, under the owner's standing blanket approval for the post-v0.1.0
rounds. This ADR does not relax the rule for anything the suites can reach; it writes down the
bound that was previously implicit, and makes every use of it visible.

## Context

`TESTING_POLICY.md` rule 1 reads, without qualification:

> **Every bug fix ships a regression test** that fails on the old code and passes on the fix.

That rule is the reason this repository trusts a green suite, and nothing here weakens it for the
code it was written about — DSP, state, serialization, the parameter surface. The problem is that
the rule is stated as though it is always *satisfiable*, and it is not. The 0.1.4 round shipped
fixes in three classes where no test in `AnabasisTests` or `AnabasisStateTests` can fail on the old
code, for reasons that are structural rather than a matter of effort:

1. **Real pointer/keyboard interaction.** The pop-up dismissal shield, the pop-up lifetime
   cancellations, the keyboard-focus release and the inline-edit abandonment are all defined by
   what the windowing system delivers, in what order, while a modal loop is running. The suites
   construct the editor (`AnabasisStateTests` does, deliberately) but they synthesise no pointer
   device and run no modal loop, so the event sequence that constitutes each of these defects
   cannot be produced at all.
2. **Artifacts produced by a platform tool.** INC-005 lives in the component metadata `pkgbuild`
   writes into a `.pkg`. There is no C++ surface to assert against; the artifact does not exist
   until a macOS packaging run produces it.
3. **Platform behaviour that is not ours to fix.** KI-014 (macOS press-and-hold suppressing key
   repeat for letters and digits) has no fix to regress, only a record.

Rule 1 written as an absolute leaves an agent two options in these cases: ship without saying so,
or write a test that passes on both the old and the new code so the box can be ticked. The second
is worse — a vacuous test is a false statement about coverage, and this repository has already paid
for one (INC-004: 1039 green checks over a configuration nothing exercised).

## Decision

Rule 1 gains a bounded exception. A fix may ship without a suite regression test **only** when all
four of the following hold, and the fix's carrier states each of them explicitly:

1. **The reason a suite test is impossible**, in mechanism terms — not "hard to test". The
   statement must name what the suite cannot produce (a modal loop, a pointer device, a platform
   artifact) rather than assert difficulty.
2. **What verification was performed instead.** A build-time fail-closed assertion, a manual check
   with the steps named, or a recorded probe. "None" is a legal answer only for a fix whose whole
   content is a documentation record.
3. **What is consequently unprotected** — the regression that could now recur unnoticed.
4. **The condition under which the exception lapses.** If the suites ever gain the capability the
   first disclosure named, the exception no longer applies to that class and a test is owed.

The disclosures live **beside the record they qualify** — in the `Evidence`/`Prevention` block of
the `POSTMORTEMS.md` or `KNOWN_ISSUES.md` entry, or in the ADR — not collected in a worklog. A
sign-off recorded away from the thing it governs is a sign-off nobody reading that thing will find.

## Options

1. **Leave rule 1 absolute and ship silently.** Rejected: it makes the policy a statement the
   repository does not keep, and the reader cannot tell which fixes are covered.
2. **Leave rule 1 absolute and write passing-on-both-sides tests.** Rejected outright. This is the
   INC-004 failure mode as policy, and it is worse than no test because it reports coverage.
3. **A bounded exception with mandatory disclosure.** Chosen.
4. **Build a GUI-automation harness so the exception is unnecessary.** Not rejected — deferred.
   The Linux input probe recorded under `worklogs/` (KI-012) is the closest this repository has
   come, and it drove a real X server with synthetic pointer events. Making that a suite fixture on
   three platforms is a project of its own, and clause 4 above is what forces this ADR to be
   revisited if it ever lands.

## Consequences

- **The exception is countable.** Every use produces four sentences at a fixed location, so "which
  fixes shipped untested, and why" is answerable by reading the records rather than the diff.
- **It does not cover DSP, state, serialization or the parameter surface.** Nothing in those areas
  is unreachable from the suites, and this round demonstrates it: both behaviour changes it makes
  to stored state — the no-op preset apply and the malformed stored slot — ship with tests, and
  both were mutation-verified against a deliberately reverted fix.
- **It raises the cost of the class-1 fixes rather than lowering it.** The pop-up work is defended
  by argument from the framework's own source instead of by a test, so its comments carry the
  mechanism and the line-level reasoning that make the argument checkable. That is a heavier
  obligation than a test, not a lighter one.
- **`TESTING_POLICY.md` rule 1 now points here.** The rule and its bound cannot drift apart,
  because the rule no longer states the bound itself.

## Verification

Not a code decision, so no test. Its own compliance is checkable: every 0.1.4 record that invokes
the exception — `POSTMORTEMS.md` INC-005, `KNOWN_ISSUES.md` KI-013 and KI-014 — carries all four
disclosures, and `docs/DOCUMENTATION_COVERAGE.md` names the round's untested surface in one place.
