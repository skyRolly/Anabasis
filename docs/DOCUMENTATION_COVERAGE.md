# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

**Last updated:** for the **P0 → P1 phase boundary** (2026-07-31). `docs/DESIGN.md` **signed off**;
P0 closed; eleven ADRs Accepted and registered.

### Twelfth post-sign-off pass — the widening from last pass had a cost I had not measured

**Last pass widened `check_tables` to match a table row at any indent, and that created a
false-positive class.** CommonMark has two code-block forms and the mask only knew one: a
**four-space-indented** block is code too, and its contents were being examined as document
structure. So an author illustrating table, link or quote syntax in indented form — the ordinary
alternative to a fence — got a red CI run on a valid document. All three checks were affected;
reproduced against the committed script before changing anything.

The cause is worth stating plainly, because it is the same error twice in two passes: I widened the
match to close a *hypothetical* gap (tables nested deeper than three columns) after measuring that
**zero such tables exist**, and did not measure what the widening let in. A change with no measured
benefit and an unmeasured cost is not a safe change.

Fixed properly rather than by reverting, because reverting would restore the original blind spot:
`indented_code_mask()` now masks indented code blocks alongside fences. It is deliberately
conservative — a run qualifies only when CommonMark's own precondition holds (preceded by a blank
line, so it cannot be paragraph continuation) **and** the nearest preceding non-blank line is at
column 0 and is not a list marker. That last clause is what keeps a table nested in a list item —
also indented four or more columns — in scope. Both directions are pinned: four cases that must now
stay silent, three that must still fire (bullet-nested table, number-nested table, and an indented
run with no blank line before it, which is paragraph continuation and not code).

**Measured, not assumed:** the new mask exempts **zero lines** of the current corpus, so it removes
a future false-positive class without giving up any coverage today. That is the check I failed to
run last pass.

**The self-test count was a hand-maintained literal, and it had already drifted.**
`total = len(cases) + 2 + 5 + 3` understates itself the moment an assertion is added to one of the
three trailing blocks — which is exactly how `REPOSITORY_MAP.md` came to advertise 29 cases against
a script running 30. The total is now **counted as the assertions execute**, and the figure has been
removed from `REPOSITORY_MAP.md` and from this file's two references rather than corrected in three
places: a number duplicated across records is a number that will drift again.

**Two docstring inaccuracies, both overstating what the tool does.** Root-relative destinations were
documented as resolving "against the scan root"; `main` always passes the repository root, so
`check-docs.py docs/policies` still resolves them repo-wide — the behaviour is the more useful one
and the wording was wrong. And two real coverage gaps the reviewer identified were unlisted:
**blockquoted tables are not checked at all** (deliberately — a prescribed block often quotes a
single row, which has no separator and would be reported as a fragment; the enacted copy is checked
in the policy file), and an indented code block *inside* a list item is not masked. Both are now in
KNOWN LIMITS with their reasons.

**Noted, no change:** the `docs` job reports as skipped on same-repo PRs, so making it a required
status check works only if branch protection accepts a skipped conclusion as passing — worth
confirming with whoever configures it. Already stated in the job's comment block. **Declined,
unchanged:** the ring-read / OpenGL tension. **Confirmed by the reviewer:** invariant 2's "all four"
is not exclusive and does not contradict `prepare()` being a fifth call site — the earlier defect
was the opposite shape (an "only by" list omitting a real trigger), so the current wording is the
safe direction.

### Eleventh post-sign-off pass — one DSP-relevant omission, five precision fixes

**The one with audible consequence: `DSP_POLICY.md` invariant 8 never named the colour model.**
`DESIGN.md` §2.8 lists the genuine discrete rewires that get the asymmetric raised-cosine duck as
"OS factor/phase change, EQ position, **colour model**, preset/A-B/undo bulk swaps", and ADR-0010's
duck-routed note names `colourModel` (row 23) beside `eqPosition` (row 45). Every one of those was
in invariant 8's enumeration except the colour model — which appeared **nowhere in `docs/policies/`
at all**. Since the invariant's guard is "one per switchable path", the enumeration *is* the list of
owed tests, so the click test least likely to be written was the one no rule named. This is the same
omission shape as the phase mode two passes ago, and ADR-0004's block 8(c) had already stated the
consequence in as many words. Added to the policy and to the prescribed block together. **No new
obligation is created** — the duck already covers that path by design; what was missing was the test
that proves it.

**Four precision fixes, all of them stated numbers or stated reasons that were wrong.** The
repository treats an unevidenced justification as a defect in its own right, and each of these is
one:

- **`.gitignore`'s rationale described something the script does not do.** It said
  `check-docs.py` "leaves this behind when imported (e.g. by its own `--self-test` run)". Running a
  script as `__main__` writes no bytecode cache; the directory appears when the module is
  *imported*, which is how ad-hoc verification snippets exercise it. Reason corrected; the entries
  themselves were always right.
- **ADR-0004's Related-code line labelled four things "latency-input triggers"** while quoting
  invariant 2's "three inputs … plus the transition … all four" in the same sentence. The call
  sites were complete and unambiguous, so nothing would have been miswired — but the label folded
  the transition into the *inputs*, which `DESIGN.md` §3.3 explicitly separates. Relabelled to the
  policy's counting: three inputs, plus the transition, plus `prepare()` as a fifth call site.
- **The coverage figures in the previous entry were 61 lines stale**, having been measured before
  the final edits to the file they describe. The ratio (~97 %) is the durable claim; the absolute
  counts move with every edit to the corpus, *including edits to this file*, so they are now
  re-derived rather than quoted — a number that cannot be kept true should not be written down.
- **The lint's own KNOWN LIMITS list had gone stale in one entry and was silent on another.** It
  still claimed "code spans are matched within a single line" after the previous pass made the
  matching paragraph-wide, and it did not mention that a fence written inside a blockquote does not
  open the mask. Both corrected — a limits list that overstates coverage is worse than no list.

**One real gap closed, with the measurement to say what it did and did not change.** `check_tables`
matched pipes at up to three columns of indent (GFM's rule), so a table nested deeper — where a
container's content column commonly sits — was silently skipped. Widened to any indent, corpus still
clean. Measured honestly: the corpus contains **10 table rows at 1–3 columns and none at 4+**, so
this changes nothing today and removes a blind spot for tomorrow. Pinned with a self-test case
rather than left as a claim.

**Noted, no change:** the `docs` job is skipped on same-repo PRs (the branch push already ran it for
that SHA), so it reports as skipped in a PR's checks list, and the `workflow_call` hazard documented
for `preflight` applies to it verbatim. Both consequences are now written into the job's comment
block. Nothing gates on this job, so the failure mode is a missing check rather than a green run
with zero builds.

**Already fixed at HEAD, re-reported against an older SHA:** the phantom fence in this file, the
unclosed-fence diagnostic, and inline-code-span exclusion — all three landed in the previous commit.
**Declined, unchanged** (fifth and sixth time for two of them): the ring-read / OpenGL tension;
ADR-0006's base-rate clip trade. **Confirmed by the reviewer independently:** OQ-013's Hard Stop is
propagated to every record that names the trim-vector transport, with the precedent citations
correctly left as statements about Anamorph's code rather than authorisations.

### Tenth post-sign-off pass — the lint reported clean on a file it had not read

**The finding that matters: `check-docs.py` printed "50 file(s) clean" while skipping 1382 of the
1401 lines of this file.** Line 20 of the previous entry began, after two spaces of indent, with
three backticks — prose describing fenced blocks. CommonMark reads that as an opening fence whose
info string is the rest of the sentence, so (a) half a sentence and the two lines after it rendered
as a code block on GitHub, and (b) `fence_mask` masked every line from there to EOF. Both status
tables in this file, every link in it, every blockquote: unchecked, silently, with a green result.

That is strictly worse than having no checker. It is also the third time in this branch that
*a passing result was mistaken for a correct one* — after `--random-seed 0` and `build.sh`'s exit
code — and the second time in three passes for this script specifically. The prose defect is fixed
(the line now reads "triple-backtick fences"), and four changes make the class detectable:

- **An unclosed fence is now a finding**, not a silent exemption. `fence_mask` returns the opener's
  line number and the run reports it. An unclosed fence is a real rendering defect on its own — the
  rest of the file renders as code on GitHub — so this catches a document bug *and* closes the blind
  spot with one check.
- **Coverage is now measured, not assumed.** The exempt lines are inside genuine fenced blocks and
  no file is more than half masked, so "clean" has a denominator behind it. The *ratio* is the
  durable claim (~97 %); the absolute counts move with every edit to the corpus — including edits to
  this file — so they are re-derived rather than quoted here. To reproduce, iterate
  `markdown_files()` and sum `fence_mask()` over the tree.
- **A `docs` job runs the lint in CI** (`build.yml`), deliberately outside the `preflight` gate and
  outside every build job's `needs`: it must run in the pre-P1 scaffold, and a prose defect should
  fail the run without skipping a binary. `--self-test` runs first, because a zero exit is not
  evidence unless the script's own guarantees were exercised in the same run.
- **`--self-test` grew** to cover the shapes this review named.

**Three more false-positive classes in the same script, all reported and all real.** Inline code
spans were not excluded, so the illustration `` `[t](path "Title")` `` in the previous entry was a
"broken link" the moment the phantom fence stopped hiding it; a fence closer shorter than its opener
ended the block early; and `check_tables` only matched pipes at column 0, so every indented table —
including ADR-0003's detector-rate table and ADR-0007's schema table, exactly the nested tables where
a mid-table intrusion is hardest to see — was never examined at all.

**Fixing those exposed a fourth, and the corpus run is what caught it.** With indented tables now in
scope, ADR-0003 line 100 was reported as a table fragment: it begins `|| isMod)` — the continuation
of a code span opened on the *previous* line (`` `oversample != Off && (drive > 0.01 || isMod)` ``).
Code spans may wrap across lines within a paragraph, and the blanking was line-local. Fixed at the
root — spans are now matched over each paragraph and split back with lengths preserved — rather than
by special-casing the symptom. **This is the method paying off:** the run was not treated as done
when it exited non-zero on one file; the single finding was chased to a cause, and the cause was a
real gap rather than a nuisance to suppress.

**Two limits are now stated rather than implied** (C7), because the reviewer was right that the
docstring claimed more than the code does: link existence is checked against the filesystem, so a
case-mismatched path passes on macOS and 404s on GitHub; and reference-style links and autolinks are
not checked. Root-relative (`/docs/x.md`) and percent-encoded destinations now resolve correctly.

**Declined, unchanged** (fourth and fifth time for two of them): the ring-read / OpenGL tension;
ADR-0006's base-rate clip trade; and invariant 4's wording, again confirmed literally true.

### Ninth post-sign-off pass — the lint itself was the defect; six fixes, three declined

**Three bugs in `scripts/check-docs.py`, all mine, all shipped last pass.** The script was written to
stop a class of regression and was itself the regression: on documents it had no business rejecting,
it would have failed. It scanned clean only because no current document happens to exercise any of
the three paths.

- **No fence tracking.** Every line starting with `|` was treated as a table row, including inside
  triple-backtick fences — so any document showing table syntax as an *example* would be reported as
  a broken table. This file and the ADRs are full of quoted markup; the script's own docstring would
  have tripped it.
- **Lazy continuation over-reported.** CommonMark applies it only to *paragraph continuation text*.
  A quote ending in a bare `>` has closed its paragraph, and an ordered list starting at 1
  interrupts one — neither is absorbed, and both were flagged. The interrupter set also mistook
  prefix-matching for block detection (`*emphasis*` counted as a bullet).
- **Link titles broke link checking.** `[t](path "Title")` is valid markdown; the code stripped only
  a `#` fragment, so the destination became `path "Title"` and the file "did not exist".

Rewritten: a fence mask excludes fenced content from all three checks; `interrupts_paragraph()`
encodes the actual CommonMark interrupters (including the only-at-1 rule for ordered lists); and
`link_destination()` parses angle-bracketed destinations and all three title forms.

**A fourth bug appeared during that fix, and is the one worth recording.** The rewritten interrupter
patterns anchored at column 0 (`^\s{0,3}`), which is what CommonMark specifies — measured against the
*container's* content column. A line-based lint has no container stack, so inside a numbered ADR item
(five columns of indent) every quote line stopped counting as a quote, and the run reported **31**
findings in ADR-0004 alone. All 31 were false: each "absorbed" line was itself a quote line. Fixed by
stripping indentation before matching, which trades a few false negatives for zero false positives —
the correct direction for a check nobody is obliged to run. **The finding is the method, not the
patch:** the first version was verified only against a hand-written fixture and the clean tree, which
is exactly the "green means correct" mistake that produced `--random-seed 0` and the `build.sh` exit
code. A check must be run against the *real corpus* and every finding accounted for.

`--self-test` now pins all fourteen cases — five that must stay silent, four that must fire, five
destination-parsing forms — so the next edit to this script cannot quietly re-open any of them.

**Two residual singular descriptions of the frozen-trims transport**, the tail of the OQ-013
propagation. ADR-0011's Related-ADRs line ("restore uses **the inject atomic**") and ADR-0009's
copied-scope list (the pattern authorised for copy with no caveat, under a standing authorisation
whose only conditions are provenance and the item-5 list). Neither is as load-bearing as the
ADR-0005/ADR-0010 sentences fixed last pass, but both are records a P1 author could cite while
wiring the blocked path — and "almost fully propagated" is precisely what made the last pair
dangerous. ADR-0009's authorisation is now explicitly scoped to **single-scalar commands only**.

**Two rendering/terminology fixes.** `REPOSITORY_MAP.md`'s docs tree carried `**bold**` and backticks
*inside* the fence, where they render as literal asterisks — the same "invisible in source, wrong when
rendered" class the lint exists for, and one its three checks do not cover. Removed. (A sweep found
18 other fenced-emphasis occurrences, all of them **intentional**: the entry-format templates in
`FUTURE_RISKS`, `KNOWN_ISSUES`, `POSTMORTEMS` and `ADR_INDEX` show the literal markup an author is
meant to copy. Left alone.) And `DESIGN.md` §3.3 called the three `onChanged` callbacks plus
`setNonRealtime()` "those four … latency *inputs*", while `DSP_POLICY.md` invariant 2 says three
inputs **plus the transition** — the transition is a trigger, not an input. Same words, different
referents, in a spot this audit had just claimed to align. Re-worded to the policy's counting.

**Declined, unchanged and for the same reasons** (third and fourth time for two of them): the
ring-read / OpenGL tension; ADR-0006's base-rate clip trade; and invariant 4's wording, again
confirmed literally true.

### Eighth post-sign-off pass — six fixes, one new script; three declined

**The validation battery is now an artefact, not a habit — and the reviewer was right that it
wasn't.** Last pass claimed "a table-integrity check is now part of the validation battery", which
described a capability the repository did not have: no script under `scripts/`, no docs job in
`.github/workflows/`. That is exactly the unevidenced claim constraint C7 forbids, made by the audit
file that enforces C7. Fixed by building the thing: **`scripts/check-docs.py`** now runs three
mechanical checks over every `.md` — GFM table integrity, broken relative links, blockquote lazy
continuation — each one present because that defect shipped here at least once and was invisible in
the diff that introduced it. Verified against a fixture containing all three defects (it reports all
three, exit 1) as well as against the real tree (50 files clean, exit 0); a script that only ever
returns "clean" proves nothing. *(That verification was still insufficient — see the following pass,
where the "clean" result turned out to be false.)* It was **not** wired into CI at this point;
a **docs** job was added the pass after. Deliberately *not*
included: the ADR-prescribed-block ↔ enacted-policy comparison, which is still run by hand, because
it has documented cosmetic artefacts (headline bolding, dated attributions) and encoding those as an
allowlist would make the script assert more than it can check.

**Two Accepted ADRs still licensed the path a Hard Stop blocks.** OQ-013's gap was propagated to
ADR-0007, ADR-0011, `THREADING_POLICY.md`, `HANDOVER.md`, `README.md` and `DESIGN.md` §5.4 — but
**ADR-0005** item 10 ("the frozen vector serialized per A/B slot via the sentinel-atomic inject
pattern") and **ADR-0010** ("restored through the sentinel-atomic inject at the forced duck's silent
bottom") were missed, as was `DESIGN.md` §7. Since an ADR outranks both the policy and `DESIGN.md`,
a P1 author reading only those two records had an Accepted-ADR licence to wire the exact path the
Hard Stop forbids — the propagation being *almost* complete is what made it dangerous rather than
obviously incomplete. All three corrected, each keeping the property its own record actually needs
(per-slot travel), which holds under either candidate transport.

**Three records still described the latch sentence in its pre-fix form.** `DESIGN.md` §3.3's
sign-off summary, OQ-010's Decision paragraph and this file's "what the sign-off enacted" all said
the latch sentence "names only the oversampling factor" — the wording a previous pass corrected in
the policy and in ADR-0004's prescribed block precisely because latching the factor alone leaves a
phase switch free to move reported latency mid-block. The decision records (ADR-0004 item 4,
ADR-0003 item 4, §3.4) were always right; these three are summaries of the amendment, which is what
a reader checking "what did it change?" lands on. All now say the sentence drops the lookahead and
names **factor and phase mode**.

**Two clarifications where a reader had no sentence to cite.**

- **`dynTilt` runs at the oversampled rate**, being a sub-block of Clipper/Sat, while invariant 5
  enumerates "the EQ" as base-rate and grants exactly one exception. The reading was always
  defensible — invariant 5 enumerates *chain stages*, and "the EQ" is the `eqPosition`-mobile stage,
  not every filter in the path — but nothing said so. Noted in ADR-0002 item 2, where `dynTilt`'s
  not-a-stage status is already established, so **no policy text changed** and no new exception was
  created: `dynTilt` needs none, because the estimator's exception exists for a stage-external tap
  and `dynTilt` is not one.
- **`DESIGN.md` §3.3's PDC bullet read as an exhaustive call-site list** while naming only the four
  latency inputs. Aligned with ADR-0004's phrasing: four inputs, `prepare()` counted separately as
  the host re-establishing the model. Same shape as the miscounts fixed twice before, caught this
  time before it became one.

**Declined, unchanged from last pass and for the same reasons:** the ring-read / OpenGL tension
(acknowledged in ADR-0011, deferred to `THREAD_MODEL.md` at P1); ADR-0006's base-rate clip under a
dBTP tolerance (the ADR states the limitation itself; the guarantee is claimed only for
estimate-plus-backstop, and the P2/P3 gates would expose it); and invariant 4's wording, which the
reviewer again confirms "remains literally true" — re-wording a correct rule against a hypothetical
future edit is how two of the recent regressions started.

**Confirmed, no change:** the reviewer's independent re-derivation of the 49-row surface, the nine
non-automatable flags, the fixed-point property, every curve maximum, the 7/2/1 managed-set split,
Tape Glue's 40 % cap, the interpolator group delay, the wireframe arithmetic and the ten `int_`
fields; the full prescribed-block sweep; and — now scripted — that the OQ-013 note sits below the
permitted-path table with all seven rows intact.

### Seventh post-sign-off pass — seven fixes, two of them self-inflicted; three declined

**The worst defect in this set was invisible in the source and only existed when rendered**, which
is a class this audit had not hit before and now checks for mechanically.

- **A blockquote was inserted *inside* the permitted-path table** in `THREADING_POLICY.md` — the
  OQ-013 gap note added last pass landed after the fourth row. In GFM a table cannot resume after an
  intervening block, so the last **three** permitted paths (SPSC ring, meter atomics, staleness
  counters) rendered as a paragraph of pipe-separated text with no header, outside the table their
  own header governs. The source diff looked entirely reasonable; nothing but rendering shows it.
  Note moved below the table, immediately after "Any path not in this table is a new cross-thread
  path → Architecture Review Gate" — which is also where it belongs logically, since the missing
  edge *is* an instance of that sentence, and where both records already said it was ("under the
  table"). A table-integrity check was added to the validation battery in response — see the
  following pass, where it became an actual script rather than a described habit.
- **The PDC amendment note miscounted the recompute triggers** — it said "two of the **four**
  recompute triggers … `prepareToPlay` and `setNonRealtime()`", which puts `prepareToPlay` inside the
  canonical four and silently displaces one of the three host-hidden latency inputs. This is the
  *same* miscount a previous pass fixed in ADR-0004's Related-code line, reintroduced by a note
  written to fix something else. Fixed by **removing the count entirely** rather than correcting it:
  the sentence now reads "two of the recompute call sites ADR-0004 item 5 mandates". A number that
  does not appear cannot drift out of step with `DSP_POLICY.md` invariant 2. Applied to the policy,
  ADR-0011's prescribed block, ADR-0011's prose and this file's own narrative of the last pass.

**One more count, and three residual-reading fixes.**

- **`MODE_AND_ADAPTATION_POLICY.md` announced "Two consequences" and listed three.** The third is the
  macro-curve freeze — the obligation `RELEASE_COMPATIBILITY_CHECKLIST.md` and
  `PARAMETER_COMPATIBILITY_POLICY.md` rule 7 both rest on, so a reader checking against the stated
  count could have treated the one load-bearing bullet as stray. Now "Three".
- **ADR-0007's Decision body still read as if the trim transport were settled.** The gap is
  propagated everywhere else, but the bullet said "a sentinel-valued atomic" with the disclaimer
  trailing as a parenthetical — and since an ADR outranks both `DESIGN.md` and the policy, that
  singular would have won for anyone reading only the Decision. The disclaimer is now *in* the
  sentence: the transport is explicitly not fixed by that record, with OQ-013 and the Hard Stop
  named inline.
- **ADR-0002 asserted "Nothing else in invariant 1 changes"** while the enacted invariant carries a
  rationale paragraph and an amended `Guarded by:` line its prescribed block does not. Neither is
  unauthorised — the rationale restates the ADR's own item-2 argument, the guard change is item 7 —
  but ADR-0003 already carries a "scope of verbatim" note for exactly this situation and ADR-0002
  did not. Note added; no text moved.
- **`README.md` named two of the three P1/distribution blockers** and then deferred to the list of
  record. The pointer keeps it from drifting, but naming two of three reads as exhaustive. OQ-013
  added.

**Declined, with reasons.** The ring-read / OpenGL tension (reviewer agrees it is acknowledged, not
hidden; ADR-0011 defers it to `THREAD_MODEL.md` at P1). ADR-0006's base-rate clip under a dBTP
tolerance (the ADR states the limitation itself; the property is claimed only for the
estimate-plus-backstop combination, and `testOutputNeverExceedsCeiling` plus the invariant-11 matrix
are the gates that would expose it). Invariant 4's wording, which the reviewer notes remains literally
true and which no record contradicts — re-wording a correct rule to pre-empt a hypothetical future
edit is how the last two regressions started.

**One item the reviewer raised as a note became a decision worth recording.** `eqPosition` and
`colourModel` are automatable *and* duck-routed, so a stepped automation lane produces repeated
~34 ms dips. That is not an invariant-8 violation — the duck **is** the click-free mechanism, and a
smooth deliberate dip is not a level jump — and it does not make them candidates for the
non-automatable set, because unlike `lookahead` (a live read offset that cannot be swept at all)
a stepped rewire is well-behaved at every value it visits; only its *rate* is unmusical. Recorded in
ADR-0010 as supported-but-audible so P4 does not file it as a defect, with the note that changing
either flag after v0.1.0 is a `kVersion` bump + ADR.

**Confirmed, no change:** the reviewer's independent re-derivation of the 49-row surface, the nine
non-automatable flags, the fixed-point property, every curve maximum, the managed-set split by
driver (7/2/1), Tape Glue's 40 % cap, the interpolator group delay, the wireframe meter arithmetic,
the ten `int_` fields, and the prescribed-block sweep — all agree with this audit's own figures.

### Sixth post-sign-off pass — three fixes, one of them self-inflicted; one new open question

**The first item is a defect this audit's own previous pass introduced**, and it is worth recording
as such rather than as a neutral finding. The pass before this one added a seventh row to
`THREADING_POLICY.md`'s permitted-path table to cover the sentinel-valued command atomic. The row
said "carrying a value … **one value per slot**", named the **frozen trim vector** as its use, and
closed by excluding anything "multi-word" — three clauses that cannot all hold, because the trim
vector is **four** scalars (release, stereo-link, sidechain-HPF, dynamic-tilt — `DESIGN.md` §5.4,
ADR-0005 item 10). Read strictly the frozen-trim restore had no permitted mechanism at all; read
loosely, an implementer publishes four independent atomics with no ordering and consumes them
half-updated — and a half-consumed vector is a **permanently** half-restored slot, so a frozen A/B
slot renders differently from the slot that was saved, defeating the bit-repeatability
`MODE_AND_ADAPTATION_POLICY.md` invariant 3 requires of Freeze.

The cause is worth naming because it is the pattern behind most of the recent regressions: the row
was written to make ADR-0011's "every edge is in the table" claim true, and the claim was allowed to
drive the rule instead of the other way round. The `abMatchGain` precedent carries **one float**;
extending it to a vector was an inference, not a reading.

- **Fixed by narrowing, not by inventing.** The row now covers **one scalar**, which is exactly what
  the precedent establishes. The trim-vector transport is *not decided* — choosing between *N*
  parallel sentinel scalars with a stated ordering guarantee and a single release/acquire-gated
  per-slot POD is a thread-model decision (Architecture Review Gate + ADR + Hard Stop). It is raised
  as **OQ-013**, carried as an explicit gap in `THREADING_POLICY.md` under the table, excepted from
  ADR-0011's compliance claim, and pointed at from ADR-0007, ADR-0011 §Decision and `DESIGN.md` §5.4
  so no record reads as if the mechanism were settled. **P1 may not wire that restore path**;
  nothing else in the P1 skeleton depends on it.

**Two genuine defects in the accepted set, both created by an edit that removed one thing and
silently removed a second.**

- **`DSP_POLICY.md` invariant 2's latch sentence had lost the phase mode.** ADR-0004's amendment
  removed "or lookahead" from "an oversampling-factor **or lookahead** change is latched" and left
  only the factor — but reported latency is `maxLookahead + osLatency(factor, **phaseMode**)`, and
  linear-phase FIR stages carry group delay the minimum-phase path does not. Every other record has
  it right (ADR-0004 item 4, ADR-0003 item 4, `DESIGN.md` §3.4 all say "factor **or phase**"); the
  binding policy did not, so a P1 author following the highest-ranked rule could apply a phase switch
  immediately and move reported latency mid-block. Restored in the policy **and** in ADR-0004's
  prescribed block 8(a), which must stay verbatim-identical.
- **Invariant 8's click-free enumeration was missing the phase mode too**, while ADR-0003 item 9
  requires "OS factor and phase each get their own click-free path test" — so the test most likely
  to be skipped was the one the enumeration did not name. Added to the policy and to ADR-0004's
  block 8(c), carried there rather than in a new ADR-0003 block because two ADRs prescribing the
  same sentence differently is precisely the divergence the block format prevents.

**One rule that no implementation could have obeyed.**

- **"PDC/latency must be recomputed on the message thread"** — in `THREADING_POLICY.md`'s
  forbidden-access list and in ADR-0011's Decision ("recomputed **only** on the message thread").
  Neither `prepareToPlay` nor `setNonRealtime()` is a message-thread callback; hosts call them from
  their own setup/processing threads, and ADR-0004 item 5 mandates **both** as recompute triggers.
  The rule therefore forbade two of the call sites it required. The substance was never thread
  identity — it is that the predictor is `const` and race-free, that there is a single
  `setLatencySamples` call site, and that nothing recomputes PDC from `processBlock` — so both
  records now say **off the audio thread, never inside `processBlock`**, enacted as a prescribed
  block in ADR-0011. No property the original protected is lost.

**Unchanged, as agreed with the reviewer:** the ring-read / OpenGL-context tension. Explicitly
acknowledged in ADR-0011, deferred to `THREAD_MODEL.md` at P1.

**Also this pass:** four earlier-reported items were re-checked and found already fixed at HEAD
(ADR-0004's PDC trigger list and its Consequences sentence, the release checklist's macro-mapping
rationale, ADR-0007's conditional factory-mask rule, and the PR description's sign-off framing). The
phrases quoted against them survive in exactly one place — this file's own narrative of the passes
that fixed them. **No edits were made to any of the four.** Rewriting text that is already correct is
how several of the recent regressions entered, including the one at the top of this entry.

### Fifth post-sign-off pass — seven fixes, three confirmations

**The first defect in this set would have failed the first real build**, which is a different class
from anything the previous four passes found.

- **ADR-0008's target graph omitted `juce::juce_opengl`.** `DESIGN.md` §6.1 mandates an
  `OpenGLContext` attached on macOS and Windows (never Linux/X11), ADR-0009 carries that as
  copied-in scope and ADR-0011 reasons about components painting on that context — but the ADR that
  *owns the CMake target graph* listed the plugin's `PRIVATE` links as `AnabasisDSP` /
  `juce_audio_utils` / `juce_dsp` only. An ADR outranks `DESIGN.md`, so a P1 author working from the
  highest-authority record writes a `CMakeLists.txt` that either fails to compile the GL path or
  silently ships without it on the two platforms that use it. Fixed on **both** targets that need
  it: the sibling links it `PRIVATE` on its plugin target *and* its state-test target
  (`Anamorph:CMakeLists.txt:206,271` [Verified]), the second because the state test compiles the GUI
  sources and therefore the editor's context member — an omission there is a link error, not a
  missing render. `BUILD.md` §"One source list, two consumers" now says so too.
- **ADR-0008 said "Four declared targets" and enumerated five.** Item 4 declares two console apps.
  The miscount was inherited verbatim from `worklogs/2026-07-30-p0-anamorph-research.md:394`, which
  numbers to (5) under a "Four" heading — corrected there as well, since it is the evidence file
  every build claim cites. `AnabasisStateTests` is the one target that compiles the wrapper sources,
  so a reviewer checking against the stated count could have passed the build with the whole
  state / parameter-compatibility suite unbuildable. Same class as ADR-0004's trigger list last pass.

**Two policy records that outlived their reasons — the pattern this project keeps producing.**

- **`PARAMETER_COMPATIBILITY_POLICY.md` rule 7 still justified the macro-curve freeze by
  automation.** `MODE_AND_ADAPTATION_POLICY.md` invariant 6 was re-grounded on **recall** because
  the macros are non-automatable (ADR-0005/ADR-0010) and a lane on a managed parameter writes it
  directly without consulting the mapping — but rule 7 sits at the *same* authority level, so the
  corrected record did not outrank the uncorrected one, and a maintainer could have concluded the
  freeze does not apply and waved a post-release curve change through. The obligation was never in
  doubt; only its reason was false. Rule 7 re-grounded, enacted by a prescribed block appended to
  ADR-0005 (`ADR_POLICY.md` rule 5), and invariant 6's now-stale "independent of rule 7's automation
  framing" aside updated to point at the corrected rule.
- **ADR-0011 claimed conformance to a permitted-path row that did not describe its edge.** Its
  Consequences said every cross-thread edge "is one of its six rows", but the Decision routes the
  **frozen trim vector** (ADR-0007) through sentinel-valued per-slot inject atomics that *carry a
  value*, while the nearest row is a payload-free `atomic<int>` where the arrival is the whole
  message. The policy says "Any path not in this table is a new cross-thread path → Architecture
  Review Gate", so the claim was self-refuting. Rather than qualify it, ADR-0011 now **enacts a
  seventh row** for the `abMatchGain` idiom — value plus sentinel in one `exchange`, one writer, one
  consumer, a compile-time-bounded slot set — with the boundary stated explicitly so the row cannot
  be read as a licence for a general message queue (which *is* a thread-model change). Settled now
  because `THREAD_MODEL.md` is generated from this ADR at P1.

**Three smaller ones.**

- **ADR-0010's exclusion-tier table defined `preset-excluded` as "view tier + `freeze`"**, which made
  its "travels in A/B and undo" column false for the four view-tier members swept in — only `freeze`
  travels. A reader building the shared predicate from that row alone could have let `advancedMode`
  into A/B and undo, which is the X11 editor-resize crash path the next paragraph exists to prevent.
  Rows now list disjoint membership with the `view ∪ {freeze}` predicate stated separately — the
  phrasing `DESIGN.md` §4.2 and ADR-0004 already used ("*Preset-excluded* adds `{freeze}`"), so the
  tier name is unchanged and nothing else in the corpus moves.
- **The mandated true-peak matrix has one degenerate cell.** At OS Off no filter is instantiated, so
  `int_osPhase` cannot reach the estimator and `Off × linear` duplicates `Off × minimum`. Kept — a
  uniform sweep is harder to get wrong than a hand-pruned one — but named in ADR-0003 item 9 and in
  `TESTING.md` so a P3 implementer neither hunts for a phase difference at Off nor prunes further.
- **OQ-001 ended with the obligation the same sentence says is discharged** ("…and recorded in the
  P0 build-decision ADR", after "**Recorded by ADR-0008**"). OQ-003's sibling edit dropped that
  trailing clause correctly, so this was an inconsistency between two edits rather than a decision.

**Confirmed, no change:** the §4.2 arithmetic and count sweep; the prescribed-block ↔ enacted-policy
comparison (the emphasis/attribution differences are the artefact ADR-0003 now documents); and
`HANDOVER.md`'s Blocking-P1 claim. **Still open by design:** the ring-read / OpenGL-context tension —
the reviewer agrees it is acknowledged rather than hidden, and ADR-0011 defers it to `THREAD_MODEL.md`
at P1 rather than amending the policy's single-reader phrasing without grounds.

### Fourth post-sign-off pass — eight fixes, three confirmations

The theme this pass is **records that still describe a decision as open after it was taken**. Three
of the eight are that exact shape, and the worst of them sits in a *binding rules* file.

- **`THREADING_POLICY.md` still let a contributor choose where the adaptive engine runs.** Its
  "Adaptive engine — where it runs" clause deferred the placement to "an ADR before implementation";
  ADR-0011 took that decision and its Consequences section says in so many words that the clause "is
  discharged here". A P4 author reading only the policy — the file that is binding from day one —
  could have implemented a placement the Accepted ADR forbids. The clause now states the decided
  split: **feature extraction and adaptive trim slewing on the audio thread** inside the ≤ 0.5 %
  budget, **macro mapping (MacroEngine) on the message thread only**, with either alternative called
  out as a Hard Stop. §"Current model" gained the ADR-0011 pointer it lacked, so the `TODO (no code
  yet)` no longer reads as *no model yet*.
- **`AI_AGENT_POLICY.md` still routed the shared-module question to OQ-005**, which is `Resolved`.
  ADR-0009 chose copy-and-adapt with provenance headers, no shared module for v1, and a named
  revisit after v0.1.0 — so an agent proposing a shared module was contradicting an Accepted ADR
  while believing it was answering an open question. Now points at the ADR and marks the proposal a
  Hard Stop.
- **`ADR_INDEX.md` rated ADR-0008 `Partially Verified` on sibling-repo evidence.** The JUCE pin and
  the identity codes were read out of *Anamorph*, which is evidence about how Anamorph builds, not
  about how Anabasis does — Anabasis has no `CMakeLists.txt` to verify against. Downgraded to
  `Unverified` with that reasoning recorded, so the rating no longer borrows another repository's
  confidence.

**Two count/reference defects in implementation checklists** — the class of defect the previous pass
named, so they were looked for deliberately.

- **ADR-0004's Related-code line said "all four triggers" and then listed five.** The canonical four
  are the three host-hidden latency inputs plus the realtime→offline transition (`DSP_POLICY.md`
  invariant 2 states it that way); `prepare()` is a fifth call site but not a latency *input*. Split
  and labelled, so the ADR and the policy now count the same things.
- **`DSP_POLICY.md` invariant 5 carried a dangling `(item 6:` reference** that read as a
  self-reference to invariant 6 of the same policy rather than to ADR-0003 decision item 6. Fixed in
  the policy **and** in ADR-0003's prescribed amendment block, so the verbatim ADR↔policy match is
  preserved.

**Two transcription gaps and one typo.**

- **`TESTING.md`'s true-peak row dropped two of ADR-0003 item 9's axes** — *both phase modes* and the
  **clamp's tap** as distinct from the limiter's detector. A suite built from the procedure alone
  would have swept the OS factor and called the invariant covered, which is precisely the vacuous
  pass the mandated-stimulus table exists to prevent. Both axes restored, with the source narrowed
  to `ADR-0003 item 9`.
- **`DESIGN.md` §2.3's compressor sentence** had an unbalanced parenthesis and a stray line break —
  its third rewrite in as many passes, now closed out.

**One thing the mechanical check turned up that the review did not.** Diffing every ADR-prescribed
block against the enacted policy text byte-for-byte — rather than reading them — showed ADR-0003's
invariant 2 and 5 blocks differing from `DSP_POLICY.md` in emphasis and attribution only: the
policy bolds each invariant's opening sentence as a headline, stamps the dated
`(ADR-0003, 2026-07-31)` attribution rule 5 requires, and carries ADR-0004's later emphasis on
`**lookahead allowance**`. No clause differs. Left as it is — forcing byte-identity would make the
ADR reproduce the policy file's formatting conventions — but ADR-0003 now states the scope of
"verbatim" explicitly, so the next person to run this diff reads a recorded artefact instead of a
fresh divergence.

**Confirmed, no change:** the arithmetic and count sweep again (49 rows, nine non-automatable, the
fixed-point property, curve maxima, interpolator group delay, ten `int_` fields); and
`HANDOVER.md`'s Blocking-P1 claim against `OPEN_QUESTIONS.md`. **Deliberately not changed:** `THREADING_POLICY.md`'s ring rule ("no reads off
the message thread") versus ADR-0011 §6.1's OpenGL attachment. ADR-0011 records the tension and
explicitly declines to amend, deferring it to `THREAD_MODEL.md` at P1 — amending the rule anyway
would be a policy change with no ADR behind it (`ADR_POLICY.md` rule 5). A non-normative note now
sits under the rule so a contributor meets the nuance where the rule is, without the rule moving.

### Third post-sign-off pass — eleven fixes, one confirmation

Two of these are the *third* time the same fact needed propagating, which is itself the finding:
**a cross-record correction is not done until every record that carries the claim is fixed, and the
implementation checklists count as records.**

- **ADR-0004's Related-code list still named two of the four PDC triggers.** Its decision body was
  corrected last pass and its own correction note explains why the omission would ship a misaligned
  bounce — but the planned-code section a developer actually works from still read "PDC recompute on
  `int_oversample` / `int_osPhase`". Now all four.
- **`DESIGN.md` §3.3's "Remaining rules" bullet still gave the retired PDC-spray reason** for
  lookahead non-automatability, which footnote ³ and ADR-0004 had already replaced. It was also
  wrong for the OS controls in a second way: they are **host-hidden**, so "not automatable"
  understates them — they are not in the parameter tree at all. Rewritten to separate the two cases.

**Two governance gaps where an ADR mandated something without carrying its escape or its
amendment.**

- **ADR-0008 mandated `GIT_SHALLOW TRUE` + a commit SHA with no caveat.** `DEPENDENCY_POLICY.md`
  records that pairing as a documented CMake trap whose resolution is *drop `GIT_SHALLOW`, never the
  SHA* — but an ADR outranks a policy, so an author following the higher record had no sanctioned
  escape and the first real build could fail to configure with no permitted fix. The caveat and the
  fallback are now in the ADR itself, with the reason they must live there.
- **Two policy edits had no prescribed amendment block.** `DSP_POLICY` invariant 8's lookahead entry
  and `MODE_AND_ADAPTATION_POLICY` invariant 4's re-grounding were substantively authorised by
  ADR-0004 and ADR-0005 but not carried as verbatim blocks, unlike the invariant 1/2/5 edits — and
  this change set treats ADR/policy divergence as a defect. Both ADRs now carry prescribed blocks;
  ADR-0005 gained a *Policy amendments enacted by this ADR* section it was missing entirely.

**A rendering defect with a real consequence.** ADR-0011's same-day correction note ended mid-line
and the following unquoted lines were absorbed into it by Markdown lazy continuation — so two
sentences of **binding** contract (which thread owns the latency figure, and the dry-fill gate)
rendered as part of a historical aside a reader could dismiss. Blockquote closed; a scan of all
eleven ADRs found no other instance.

**Four smaller ones.** §10's ADR-0002 and ADR-0004 rows still read "**Must amend** … Hard Stop,
human review required" for amendments that landed at sign-off. §3.4 still framed the invariant 2
re-phrasing as future work while §3.2 had been switched to a "**Done:**" note for the analogous
ADR-0003 case. The compressor justification had its reason stated twice in one sentence — the
previous pass fixed the garbling and left the duplication. Invariant 5 diverged from ADR-0003's
prescribed text in two cosmetic spots (`ADR-0003:` for `item 6:`, and a re-worded closing clause);
now verbatim, with the attribution moved outside the quoted block. The release checklist's latency
item still said "at both ends of the lookahead range" where ADR-0004 requires **every** value —
a release run following only the checklist would have exercised the weaker property. And the blank
lines left by excising the three resolved entries were collapsed, since `README` now points at that
file instead of enumerating entries itself.

**Confirmed, no change:** the arithmetic and count sweep, for the third time — 49 rows / nine
non-automatable, the fixed-point property, curve maxima inside range, interpolator group delay, the
ten `int_` fields, wireframe meter values, and the cited worklog's existence.

### Second post-sign-off pass — eight fixes, one confirmation

The offline-render PDC gap was fixed in the *wrong two records first*. The previous pass corrected
ADR-0011 and `DESIGN.md` but missed **ADR-0004**, which is the record that actually owns the
latency contract and outranks `DESIGN.md` — its decision item 5 still said recomputation is
triggered "**only** by `prepare()` and by the `int_oversample` / `int_osPhase` `onChanged`
callbacks", contradicting its own next sentence, and its Consequences repeated "OS factor and phase
mode are the only remaining latency sources". A P1 author reading the highest-authority record
would still have shipped the misaligned bounce. Fixed there, and the fourth record with the same
hole — `DSP_POLICY.md` invariant 2 — now names all three inputs plus the realtime→offline
transition. The lesson is worth stating: *fixing a cross-record contradiction means fixing every
record that carries it, starting with the most authoritative*, not the one where it was noticed.

**Two more amendments had landed only on one side.** The release checklist's macro-mapping gate
still justified itself with "so a recorded macro-automation lane still sounds the same" while
citing the very invariant that had just been rewritten to say such a lane cannot exist — a gate
item a reviewer could dismiss as inapplicable, letting a changed curve ship. Re-grounded on recall,
with an explicit "do not dismiss this on the grounds that no such lane is possible". And ADR-0007
asserted unconditionally that "factory presets ship an all-clear mask", contradicting ADR-0005's
conditional rule at equal authority; ADR-0007 now carries the conditional form and an explicit
scope note (it owns where the mask is *stored*; ADR-0005 owns the rule).

**The verbatim-amendment test, applied to myself.** The previous pass treated an ADR/policy
*paraphrase* divergence as a defect. The same test showed `DSP_POLICY` invariant 5 carrying a
substantive clause — the true-peak-estimator exception — that ADR-0003's prescribed block never
authorised. Resolved by extending the ADR's prescribed text so the enacted policy and the text its
ADR authorises match verbatim, rather than deleting correct material from the policy.

**Three smaller ones.** ADR-0005's factory-preset bullet still restated the stronger unconditional
form two sentences after the conditional one — the exact shape the previous pass claimed to have
collapsed; now one statement. A compressor-justification sentence in `DESIGN.md` had been garbled by
an earlier edit into "confirmed or refuted by the P2 aliasing measurement *if the null tests say
otherwise*", which makes the verification contingent on an unrelated test; rewritten with the
band-limited-gain-signal reason ADR-0003 states cleanly, and with the consequence if it comes out
otherwise (the compressor moves inside the OS region — an ADR-0003 amendment).
`MODE_AND_ADAPTATION_POLICY` now names the **binary** alongside `testMacroDefaultIsFixedPoint`, so
the deliberate placement exception (behavioural guard in the state suite, because only that target
links the wrapper) is not "fixed" by a later contributor moving it somewhere it cannot compile.

**Confirmed, no change:** the arithmetic and count sweep again — 49 rows / nine non-automatable,
the fixed-point property, curve maxima inside their ranges, the interpolator group delay, the ten
`int_` fields, the wireframe meter values, and that the worklog every ADR cites exists.

### Post-sign-off review pass — thirteen fixes, one confirmation

The sign-off changed what is true; this pass fixed the places that had not caught up, plus one
genuine technical gap between two Accepted ADRs.

**The one that would have shipped a defect: offline renders reported the wrong delay.** ADR-0004
decision item 5 makes `int_offlineQuality = Force Max` render an offline bounce at 16× with the
reported figure under `isNonRealtime()` using the forced factor — so it is a **third** input to
reported latency. ADR-0011's PDC section listed only the OS factor and phase as recompute
triggers, and `DESIGN.md` §4.3 had the same gap. A P1 author following ADR-0011 would have wired
no recompute for a Force-Max change and, in hosts that do not re-`prepare` on entering offline
render, the host would compensate for the *live* factor while the render ran at 16× — a bounce
time-shifted against the rest of the project. Both records now list all three fields **plus
`setNonRealtime()`**, which is the only callback guaranteed to fire on that transition. ADR-0011
carries the correction inline as a same-day consistency fix against ADR-0004, not a reversal
(`ADR_POLICY.md` rule 4 governs reversals; this was an omission).

**A binding rule was justified by a scenario this architecture makes impossible.**
`MODE_AND_ADAPTATION_POLICY` invariant 6 froze the macro curves on the grounds that changing one
alters how a recorded automation lane plays back. It does not: the macros are non-automatable, a
lane on a managed parameter writes that parameter directly, and `M` is evaluated only on a macro
gesture. A maintainer checking the stated reason would find it false and could conclude the freeze
does not apply. The real argument is **recall**: every saved session and preset stores a macro
position, and re-mapping that position through a new curve makes a user's saved master reload
differently — a `COMPATIBILITY_POLICY` violation on its own terms, independent of
`PARAMETER_COMPATIBILITY_POLICY` rule 7's automation framing.

**Three ADR-mandated test stimuli were never transcribed.** `TESTING.md` had the test *names* but
not the stimuli each ADR flagged as load-bearing — and a name without its stimulus passes
vacuously. Now a table: `testOutputNeverExceedsCeiling` in **both EQ positions** with a +12 dB
post-limiter shelf (ADR-0002); true-peak accuracy across the **whole OS matrix**, because the
estimator's input path differs per setting (ADR-0003); the impulse landing at exactly
`maxLookahead + OS` for **every** lookahead value, and a **lookahead move** among the click-free
paths (ADR-0004). `DSP_POLICY` invariant 8's enumeration gained lookahead for the same reason —
it is the one switchable path with neither a duck nor a latch, so it is the one most likely to be
skipped.

**Two policy/ADR literal divergences.** ADR-0002 quotes its amendment verbatim; the policy had
paraphrased it, so a reader diffing the two would see an incomplete amendment — the prescribed
sentence is now used. And invariant 5's "metering taps stay at base rate" appeared to contradict
ADR-0003's estimator reading oversampled signal at ≥ 4×; the true-peak estimator is now named as
the explicit exception, with the reason (it is a measurement tap, not an audio capture point).

**Pre-sign-off language still standing in a signed-off document.** `DESIGN.md` still said
`ADR_INDEX.md` is empty and its §10 table the only record, still headed §10 "Proposed initial ADR
set", and still promised amendments to `DSP_POLICY` as future work in two Hard-Stop blockquotes —
all false as of the same change set. The header's drift disclaimer covers divergence from *shipped
behaviour*, not false claims about current repository contents.

**Four smaller ones.** ADR-0005 said a macro gesture is "nine parameter writes" — the managed set
splits by driver, so it is seven for `loudness`, two for `tone`, one for `character`. The factory
preset mask rule said both "all-clear" and "non-clear" four sentences apart; now phrased once,
conditionally. RISK-007 pointed at OQ-005, which this change set Resolved — re-aimed at ADR-0009
and its scheduled post-v0.1.0 revisit. `README`'s open-question summary omitted OQ-009; replaced
with a pointer to the list of record so it cannot drift again. The release checklist's `Ref:` line
still contemplated lookahead "gaining an explicit off position", which ADR-0004 forecloses.

**Confirmed, no change:** the count and arithmetic sweep — 49 rows with nine non-automatable
(matching ADR-0010), the fixed-point property across all nine managed parameters, every curve
maximum inside its declared range, the interpolator group delay inside the minimum lookahead, the
ten `int_` fields, and the wireframe meter values.

### What the sign-off enacted

The sign-off is the event the last five passes were building toward, and it changed the
repository's authority structure rather than just a status field:

- **`docs/DESIGN.md` → `Accepted`**, and per `SOURCE_OF_TRUTH.md` it now occupies **level 5**
  (descriptive Architecture) — outranked by every ADR it spawned, and superseded section by section
  as P1–P6 land. It is explicitly *not* maintained as a living spec.
- **ADR-0001…0011 authored, all `Accepted` and dated 2026-07-31**, registered in `ADR_INDEX.md`
  (`ADR_POLICY.md` rule 1 — an unregistered ADR is invalid). Level 3 of the authority chain is
  populated for the first time. Confidence is `Unverified` across the set by construction: there is
  no `src/`, so each is a contract the P1+ code must satisfy, upgraded as its code and tests land.
- **Three `DSP_POLICY.md` invariants amended, each by its ADR** (rule 5 — a policy changes only
  through an ADR). **Invariant 1** (ADR-0002): the chain now prints `… Limiter → [EQ (post)] →
  Ceiling → Dither` and states that the clamp is always last before dither in *both* EQ positions —
  the pre-amendment text left Post-EQ's placement relative to the clamp unstated, which a literal
  reader could take as EQ-after-clamp, making invariant 4 unsatisfiable. **Invariant 2**
  (ADR-0004): reported latency is now the **constant lookahead allowance + OS**, the latch sentence
  drops the lookahead and names the oversampling **factor and phase mode**, and the measurement-tap
  reading is asserted rather than left open. **Invariants 2 and 5** (ADR-0003): the open point is closed, so "oversampling off ⇒ no
  oversampling latency" is now asserted unconditionally. Both invariant-1 and invariant-2 changes
  were **Hard Stops** — ratified by a human, which is the only thing that clears them.
- **`MODE_AND_ADAPTATION_POLICY.md`**: invariant 4's bar on adaptation moving the lookahead lost its
  original derivation when ADR-0004 removed the latency link; it is now grounded on its own reason
  (a time-varying read offset drags the tap through the delay line). Invariant 1 gained
  `testMacroDefaultIsFixedPoint` as its named guard — the correct home for it, since it guards a
  macro-layer rule rather than a DSP one.
- **Five open questions Resolved**: OQ-001 and OQ-003 (→ ADR-0008), OQ-004 (→ ADR-0005), OQ-005
  (→ ADR-0009), OQ-010 (→ ADR-0004). `OPEN_QUESTIONS.md` keeps them in its `Resolved` section with
  the date and the ADR, per its own never-delete rule.
- **P0 closed, P1 opened**: `HANDOVER.md` is now a phase-boundary snapshot carrying the
  `DEVELOPMENT_BRIEF.md` §13 phase summary (changes, next-phase plan, current risks, C++23 canary
  status), and `CLAUDE.md`, `README.md`, `REPOSITORY_MAP.md` and `SOURCE_OF_TRUTH.md` all moved off
  "P0, no code until sign-off". The lifecycle policy's phase-completion trigger fired properly this
  time — the previous pass had correctly identified that it had *not* yet fired.

### Review items handled in the same unit of work

The final review round's two authority defects both **dissolved** once the amendments landed rather
than needing a workaround: `RELEASE_COMPATIBILITY_CHECKLIST.md`'s latency checkbox now cites the
amended `DSP_POLICY.md` invariant 2 (not the design document, which had no authority to settle it),
and OQ-010's claim that the invariant *is* phrased against the allowance became true instead of
premature. Four smaller items fixed: the audit's ADR count (said nine, is eleven) and its
test-registration instruction (which contradicted `DESIGN.md` by sending the macro guard to
`DSP_POLICY`'s map); a duplicated `AB` child in the §4.4 schema listing; footnote ⁶ printed before
⁵; two unmarked ⊕ defaults in §4.3; and §5.3's inaccurate "ties all nine managed values to one
`l`" (three of the nine are driven by `character`/`tone` — the curves are jointly coupled to the
triple, which is what makes "heavy colour + gentle limiting" unreachable).

Prior: for the **P0 design document** (2026-07-30). `docs/DESIGN.md` created —
status `Proposed`, awaiting owner sign-off (`DEVELOPMENT_BRIEF.md` §11 exit criterion).

The document was produced from a five-domain research pass over the full Anamorph repository
(source, not just docs — §24 step 1), recorded with file:line evidence in
`worklogs/2026-07-30-p0-anamorph-research.md`. It contains: the two-layer architecture and
module inventory; per-stage DSP design; the oversampling/true-peak/latency contract — including
the **measurement-tap** resolution proposed for `DSP_POLICY.md` invariants 2/5's open point
(the policy text itself is edited only when ADR-0003 is accepted, per C6); the full
**49-parameter table** plus the host-hidden state table and serialization schema v1; the macro
layer and OQ-004 coexistence argument; draft macro curves (explicitly ⊕-marked as tuning
material, C2); UI wireframes with the family-consistency contract; the OQ-005 recommendation;
the performance-budget allocation and benchmark commitment; and the proposed ADR set — **eleven**
by the end of the review rounds (0008 build/identity and 0009 code reuse added in the third pass;
0010 parameter surface and 0011 threading in the fourth). Every
value the brief does not specify is marked **⊕ proposed** so sign-off ratifies it explicitly
rather than absorbing it silently (C7).

Synced in the same unit of work: `OPEN_QUESTIONS.md` (OQ-004/005/008/010 now carry the DESIGN
recommendations, all *pending sign-off* — none moved to Resolved, since the sign-off is the
decision event), `HANDOVER.md` (all P0 work items delivered; the phase stays open until sign-off),
`REPOSITORY_MAP.md`, `README.md` §Project status. Confidence: the design document is a
*contract proposal* — nothing in it is `Verified` about Anabasis (there is no code); Anamorph
precedent claims cite file:line and are `Verified` from the research pass.

**Fourth review pass on the design document (same day).** Eight review findings fixed, plus
**seven blockers of my own making** caught by an adversarial verification pass run *before*
commit — the constant-latency decision below had a blast radius I had not propagated.

### The self-inflicted blockers (verification pass, pre-commit)

Making §3.3's latency decision was correct; landing it in one section was not. A three-lens
verification sweep found the decision half-applied, and the pattern is worth recording because it
is the failure mode of any change to a *contract*: the new statement was written, the old one was
left standing in six other places, several of them more binding than the section that changed.

1. **§3.4 still asserted the old model** ("reported latency … phrased against the engaged
   lookahead") four paragraphs below the new formula.
2. **`DSP_POLICY.md` invariant 2's binding body was falsified without being flagged.** Its latch
   sentence names "an oversampling-factor **or lookahead** change" — under the decision only OS
   latches. A reported-latency change is an AI-agent **Hard Stop** and an Architecture Review
   item, and unlike the structurally identical §1.2 case it had no Hard-Stop framing. Now carried
   as a ⚠ blockquote in §3.3, an amendment obligation on ADR-0004, and a Hard-Stop checklist line.
3. **The §10 ADR-0004 row still specified the superseded contract** — and per `SOURCE_OF_TRUTH.md`
   the ADR is the binding artifact, so a P1 author would have written the rejected model into an
   `Accepted` record that then outranks §3.3.
4. **`lookahead`'s non-automatability was frozen on a rationale the decision destroyed** ("a host
   cannot spray PDC changes" — under constant latency it cannot spray anything). The flag is
   right, the reason was not: it is non-automatable because the engaged value is a *read offset
   into a live delay line*, so sweeping it at automation rate drags the tap through the buffer.
5. **The mask move's own precedent was never actually implemented.** The previous pass said the
   frozen trim vector "travels per-slot", but only §4.2's prose changed — §4.4 still stored one
   global vector in `ADAPTIVE`, so two frozen slots would have shared it and Freeze would not have
   been bit-repeatable. Both the trims *and* the mask now live inside each `AB` slot, and the
   **undo unit was widened to match** (`StateSet {params, presetName, baseline, frozenTrims,
   detachMask}`) — otherwise undoing an edit restores the value and strands its detach bit.
6. **The §4.2/§4.3 parameter surface had no ADR at all** — the checklist asked the owner to
   ratify 49 IDs, both exclusion tiers and the lockable set, and per the authority rule that
   ratification would have bound nothing. Exactly the defect the ADR-0008/0009 additions fixed one
   section over. Added **ADR-0010** (parameter surface) and **ADR-0011** (threading model), both
   mandatory subjects under `ADR_POLICY.md`.
7. **§2.8 removed the duck from lookahead without replacing its click-free coverage.** Moving a
   read tap is not inherently click-free. The mechanism is now stated — the *audio* delay stays
   fixed at 10 ms and only the detector alignment moves — with a per-path click test owed.

Three consequences reached beyond `DESIGN.md` and were corrected as drift (C6):
`RELEASE_COMPATIBILITY_CHECKLIST.md` had "the reported value is exactly the engaged lookahead" as
a **release-gate checkbox** a correct build would now fail; RISK-008 and OQ-010 carried the same
phrasing. `ADR_INDEX.md`'s expected-first-batch list gained the three new subjects.

Two arguments were also found to be *unsound rather than merely stale*, and were rewritten to
what is actually true. §5.3's case for presets carrying the detach mask claimed it prevents a
value jump — but rule 3 re-engages detached parameters anyway and preset apply lands stored values
either way, so the mask changes **no** trajectory; what it changes is what the UI claims and what
"reset to macro" can do. And "factory presets are curve-consistent by construction" was asserted
with nothing constructing it: §5.5 ties all nine managed values to one `l`, so a heavy-colour
gentle-limiting patch is not reachable from a single macro triple. It is an authoring constraint
checked at P6, not a property. An off-curve-but-engaged residue via ungesture-d automation writes
is now stated as **accepted** rather than left implicit.


*Two decisions the sign-off is asked to approve had no decision record, so they could never
become binding.* This is the sharpest finding of the four passes, because it is created by a rule
this very change set added: `SOURCE_OF_TRUTH.md` now says DESIGN decisions "become binding only
through the ADRs it names". The §10 table named seven, and **two mandatory records were missing**
— `CLAUDE.md` §3 requires code reuse across the two products to be "recorded in an ADR, not an
ad-hoc copy" (§8 decides copy-and-adapt *now* and only promised an ADR for the *later* extraction
question), and `ADR_POLICY.md` requires an ADR for build architecture *and* format support, with
OQ-001's standing obligation naming "the P0 build-decision ADR" explicitly. Ratifying §8 and the
JUCE-pin/identity values would have bound nothing. Added **ADR-0008** (build architecture +
plugin identity: CMake structure, the JUCE SHA pin, C++20 baseline, formats, frozen identity
codes — closes OQ-001 + OQ-003) and **ADR-0009** (code reuse from Anamorph — closes OQ-005).

*Reported latency was a function of a parameter that presets carry — decided rather than
documented.* `lookahead` is in neither exclusion tier, so every preset, A/B slot and undo step
carries one, and reported latency was `engagedLookahead + OS`. Browsing presets or A/B-comparing
**during playback** would therefore change host PDC on nearly every step, and the forced duck
could not dry-fill across it (Anamorph's dry-fill engages only when latency is preserved) — a
dropout in the middle of the one workflow a mastering plugin exists for. The research pass had
flagged this as a decision Anabasis owed and §7 had copied the machinery "wholesale" without
taking it. Resolved by making the lookahead contribution **constant at its maximum**: the limiter
reads at a variable offset inside a fixed 10 ms line, so the engaged value moves freely while the
reported figure never does. Consequences are stated rather than buried — bulk swaps can now
*never* cross reported latency (OS is host-hidden and not carried by presets/A-B/undo), lookahead
needs no latch and no duck, and the price is ~8 ms of PDC nobody asked for at the 2 ms default.
That price is on the §11 checklist as its own line: it is a genuine trade, not a free win.

*The macro detach mask was global state describing per-slot facts.* It lived in the single
`ADAPTIVE` child while A/B holds per-slot parameter trees — so slot A's detached `clipDrive`
would describe slot B, and the next macro gesture would re-engage parameters the user never
detached in the active slot. Exactly the hazard the previous pass fixed for the frozen trim
vector, missed one field over. Moved into each `AB` slot. The preset half of the same hole is
also closed, and in the opposite direction from the earlier draft: presets now **carry** the mask
(⊕, reversing "cleared on load"), because a preset saved after manual edits stores off-curve
values, and clearing the mask would mark them engaged so the next macro gesture would jump them —
the fixed-point defect re-entering through recall. With it stated that preset/A-B/undo writes are
ungesture-d *and* inside the re-entrancy flag, such a preset is recallable at all: without both,
the mapper would overwrite the stored managed values from the restored macro position.

*The ADR set's `Proposed` state never actually existed.* The header said sign-off promotes the
set from `Proposed` to `Accepted`, while §10 said they are written as `Proposed` *when approved* —
approval and sign-off being the same event, the state was instantaneous and no ADR would ever be
reviewable in it. Pinned to one order: authored **directly as `Accepted`, dated at sign-off, as
the first P1 task**, each citing this document and the worklog. Writing them speculatively as
`Proposed` beforehand is what C1 forbids, and this document already fills the reviewable-proposal
role — so `ADR_INDEX.md` stays empty until sign-off and the §10 table is the interim record,
which is why it names what each ADR must settle.

*Three smaller items.* The footnote markers ran ¹ ² ³ ⁴ then jumped to ¹⁰ with no 5–9 anywhere —
a leftover that would send a P1 transcriber looking for five missing notes; renumbered to ⁵. The
wireframe frame sizes (940×720 / 940×900) are Anamorph's hard-coded constants, which the research
pass said Anabasis must replace — they are ⊕-marked, but sign-off ratifies ⊕ values, so the
reader is now told plainly that ratifying them ratifies the sibling's exact frame and that P5 is
expected to re-derive them. And the three test names this document introduces
now carry an explicit registration obligation — *corrected in the fourth pass*, because two of
the three were already registered (`testReportedLatencyMatchesImpulse` is row 2 of `DSP_POLICY`'s
map; `testModeSwitchIsSoundNeutral` is the named guard for `MODE_AND_ADAPTATION_POLICY`
invariant 2) and the genuinely new one, `testMacroDefaultIsFixedPoint`, guards a **macro-layer**
rule, so `DSP_POLICY`'s map was the wrong destination for it.

*Confirmations:* the Post-EQ/`DSP_POLICY` wording item was re-reported for the third time and
remains correctly handled (Hard-Stop blockquote, ADR-0002 obligation, checklist entry). The
measurement-tap group-delay arithmetic, the nine managed rows' fixed-point property, every curve
maximum against its declared range, the wireframe meter values against §2.9's formulas, the 49-row
count against the §4.2 heading, and every unmarked value against brief §4–§5 were all
independently re-derived with no discrepancy.

*Housekeeping:* this file's review-pass blocks had drifted out of the newest-first order the rest
of the file uses (an unlabelled first pass sat above the third and second, with its confirmations
stranded two passes below its findings). Relabelled and reordered fourth → third → second →
first.

**Third review pass on the design document (same day).** Six findings fixed, three confirmations.

*The Character macro was inert in the factory patch.* `character` drives exactly one target,
`colourDepth`, and `colourModel` defaulted to **Clean** — the *null* model, per the brief's own
`Clean ↔ Colour` framing. Applying more of nothing is nothing, so the one-knob product's
second-most-prominent control would have looked dead until the user opened Advanced and changed a
discrete parameter the macro never touches. This is the **same class** as the `clipMix` defect two
passes ago, one level up: there a managed target's *value* was not a fixed point; here the value
matched but its *audible effect* was gated by an unmanaged discrete parameter. Fixed by defaulting
`colourModel` to ⊕ `Tape` (`colourDepth = 0` keeps the default patch bit-identical either way, so
invariant 7 is untouched), documenting that Clean-makes-Character-inert is deliberate — it is the
"no colour whatever the knob says" escape — and adding the generalised rule to §2.4: *a managed
target's audibility must not be gated by an unmanaged discrete parameter at the factory default.*
Which flavour is the default is a taste call and is now on the §11 checklist.

*`advancedMode` in A/B and undo meant a compare could crash an X11 host.* It sat in the
preset-excluded tier only, so an A/B switch or an undo step would change the view mode — driving
exactly the editor resize that the table's own footnote ¹ cites as the Anamorph X11 crash (its
KI-003). Moved to the **view tier** (excluded from A/B, undo and presets; still session-
serialized), a deliberate departure from Anamorph, which lets its `advancedMode` travel with A/B.
`freeze` deliberately stays in A/B and undo — it genuinely affects the sound, so reproducing a
slot means reproducing whether adaptation was latched — with the obligation that makes that safe
now stated: **the frozen trim vector travels per-slot with it**.

*The macro-write / manual-edit discriminator was never named.* §5.2 writes managed parameters
through `setValueNotifyingHost` and §5.3 detaches a parameter when the user edits it — both land
on the same listener surface. Taken literally the MacroEngine's own writes would trip the detach
rule and every managed parameter would detach on the first macro gesture, i.e. the exact inverse
of the re-engage contract. Now pinned as two required conditions: **not macro-originated** (a
message-thread re-entrancy flag around the write burst) **and gesture-bracketed** (so automation
playback, preset apply, A/B and undo never detach — Anamorph's rule). Comparing against the
expected curve value was considered and rejected: it is a float comparison against a value the
engine is mid-glide toward, so it misfires in both directions.

*Two open dependencies lived only inside a document that gets superseded.* §11 named the
detector-delay bound and the variable-font licence as risks, deferring the register entry
conditionally — but `DOCUMENTATION_LIFECYCLE_POLICY.md`'s *new unresolved limitation* trigger
points at `FUTURE_RISKS.md` for exactly this shape, and `DESIGN.md` is superseded section by
section, so a risk parked there is a risk that disappears. Registered as **RISK-008** (latency
contract) and **RISK-009** (font licence), with §11 now citing them.

*Two smaller corrections.* The limiter has no separate threshold parameter because **the ceiling
*is* the threshold** — the conventional maximizer reading of the brief's "Gain/Threshold, Ceiling"
— now stated in §2.5 so the omission reads as a decision, since *adding* a parameter later is a
kVersion bump. And `int_spectrumOn` defaulted to off while the brief calls the spectrum
"dismissible", which implies visible until dismissed; flipped to ⊕ on.

*Confirmations:* the Post-EQ/DSP_POLICY wording item was re-reported and is already handled — the
Hard-Stop blockquote in §1.2, the amendment obligation on the ADR-0002 row, and the checklist
entry are exactly what it asks for. `SOURCE_OF_TRUTH`'s "Levels 1, 2 and 5 are empty" paragraph
gained a clause about `DESIGN.md` occupying level 5 once ratified, so it does not silently become
wrong at sign-off. The document-registration check was re-confirmed across all four places.

**Second review pass on the design document (same day).** Three findings fixed, three
confirmations.

*The section heading said 48 parameters; the table has 49.* `colourDepth` was added by the fix
above and the heading was not updated — the one artifact in the document that a P1 implementer
could have trusted over the rows, on the surface that freezes at v0.1.0. Corrected. (`HANDOVER`
and this audit already said 49.)

*The Post-EQ position is a Hard-Stop item and was not marked as one.* `DSP_POLICY.md` invariant 1
prints the chain as `… Limiter → Ceiling → Dither` and says the EQ switch moves the block "after
the limiter" — it does **not** say where Post-EQ sits *relative to the clamp*. This design reads
it as Limiter → EQ(post) → Ceiling because the literal-appended alternative makes invariant 4
unsatisfiable; a reader taking the diagram literally would read it the other way. That is
ambiguity being resolved, but resolving it still touches DSP signal order — an
`ARCHITECTURE_REVIEW_GATE` item and an AI-agent Hard Stop. `DSP_POLICY.md` is deliberately **not**
edited here (`ADR_POLICY.md`: a policy changes only through an ADR); instead the obligation is
attached to ADR-0002 and raised to the top of the §11 sign-off checklist, so a human ratifies the
reading rather than inheriting it from a diagram.

*"P0 execution is done" was the wrong claim.* `DOCUMENTATION_LIFECYCLE_POLICY.md` maps
*complete a phase* to `HANDOVER.md` + `DOCUMENTATION_COVERAGE.md` + the `DEVELOPMENT_BRIEF.md` §13
phase summary — and the brief's own §11 makes P0's exit criterion the **sign-off**, not the
delivery. So the trigger has **not** fired and no phase summary is due yet; the earlier wording
invited the opposite reading. `HANDOVER` now states plainly that P0 is *not* complete, that all
P0 work items are delivered, and that the phase-completion trigger fires at sign-off — with the
three required updates named so the next agent does not have to re-derive them.

*Confirmations:* the parameter table's macro fixed-point rule was independently re-verified across
all nine managed rows (each `M(0,0,0)` equals its declared default) together with every curve
maximum against its declared range, and all nine managed rows confirmed `Auto: yes` — consistent
with §5.2's claim that the managed Advanced parameters are the automation surface. The wireframe's
meter values were re-checked against §2.9's formulas (PLR = −1.02 − (−9.5) = 8.5; Spotify penalty
= −14 − (−9.5) = −4.5) and match. The document-registration fix was confirmed correct in all four
places, with the note that `SOURCE_OF_TRUTH.md` has no enumerated developer-class file list, so
the new §"Where `DESIGN.md` sits" is the closest equivalent — and it does more than the trigger
asks by pinning the authority rank.

**First review pass on the design document (same day).** Three findings fixed, two
confirmations.

*The new document was registered in only one of the four required places.*
`DOCUMENTATION_LIFECYCLE_POLICY.md`'s add-a-document trigger requires four updates — the
`REPOSITORY_MAP` **tree entry**, the `SOURCE_OF_TRUTH` **class list**, `README` **§Documentation**,
and this audit — and only this audit plus a prose note at the bottom of `REPOSITORY_MAP` had been
done. So the P0 deliverable was invisible to anyone following the standard navigation path, and
its authority rank was undefined. All four are now correct, and `SOURCE_OF_TRUTH` gained a
dedicated §"Where `DESIGN.md` sits": no authority before sign-off; ranked with descriptive
Architecture after it; **superseded section by section** by the ADRs it spawns, which win on any
disagreement. Getting that rank written down matters more than the index entry — ADR-0001…0007
will cite this document, and without the rule a reader could not tell which side of a future
conflict wins.

*The default patch was not a fixed point of the macro mapping.* The colour-amount curve
(`character · (0.4 + 0.6·l)`) evaluates to **0** at the default macro position, but its declared
managed target was `clipMix`, whose default is **100 %** — so the first touch of the big knob
would have collapsed the clipper blend from fully wet to nearly dry, an unexplained jump in the
factory patch. Root cause: `clipMix` is a *parallel dry/wet blend* and was the wrong target for a
*colour amount*. Fixed by giving the colour amount its own parameter — `colourDepth` (row 49,
⊕ 0…100 %, default ⊕ 0) — and removing `clipMix`/`compMix` from the managed set entirely. The
general rule the defect exposed is now stated as binding in §5.5: **for every managed parameter,
`M(0,0,0)` must equal that parameter's declared default**, verified by inspection across all nine
managed rows and guarded by `testMacroDefaultIsFixedPoint`. A P4 curve revision that breaks it is
a defect, not a taste choice.

*Freezing display names contradicted a policy that outranks the design document.*
`PARAMETER_COMPATIBILITY_POLICY.md` rule 2 explicitly permits renaming a user-facing name at any
time while the ID stays fixed, and the same policy advises decoupling the ID vocabulary from
display wording *precisely so* copy stays revisable under C8. DESIGN.md said names "freeze in the
P1 registry snapshot". Softened to the accurate reading: sign-off ratifies the names as **launch
wording**, IDs/ranges/defaults freeze at v0.1.0, and a later rename is rule 2's normal workflow
(registry + a `Changed` CHANGELOG entry + a deliberate snapshot re-freeze).

*Confirmations:* the reviewer independently re-derived the BS.1770-4 interpolator group delay
((48−1)/2 = 23.5 upsampled ≈ 5.9 base samples ≈ 0.122 ms at 48 kHz, inside the 0.5 ms minimum
lookahead) and confirmed the measurement-tap latency claim is arithmetically sound with the P2
impulse verification correctly scheduled; and cross-checked every unmarked value in the parameter
table against the brief §4–§5 plus every macro-curve endpoint against its declared range — all
consistent. The wireframe's illustrative meter values were made self-consistent with the formulas
they sit beside (I −9.5 LUFS, TP −1.02 dBTP ⇒ PLR 8.5; Spotify penalty −14 − (−9.5) = −4.5),
since a sign convention read off a mock-up is the kind of thing an implementer copies.

Prior: for the **tenth review pass** (2026-07-30). Four findings fixed, three
confirmations. **This is the closing pass of the P0 scaffolding work** — see the note at the end.

**The hex replay recipe works; the trap is next to it.** The review suspected `--random-seed
0x4aeacb4` would parse as `0` — pluginval's "generate a random seed" sentinel — making the
documented way to reproduce a randomise-only failure silently reproduce nothing. It does not:
`CommandLine.cpp` branches on `startsWith ("0x")` and calls `getHexValue64()`, and 1.0.4 round-trips
`0x4aeacb4` exactly. But reading it surfaced a real adjacent trap: the whitelist it is validated
against, `containsOnly ("x-0123456789acbdef")`, is **case-sensitive**, so an uppercased
`0X4AEACB4` is rejected with exit `-1` — the one code both scripts misclassify as an abnormal
termination and retry three times. Documented, with the decimal form as the alternative.

**The PE parser's hardening was only partial.** The previous pass bounds-checked the CodeView
record but left `e_lfanew`, the section table and the debug-directory array indexed unchecked, so a
truncated image still threw a raw .NET `IndexOutOfRangeException` instead of the diagnosable error
the function exists to produce. All four offsets are now checked in the same style. Diagnostics
quality, not correctness — these images come from MSVC in the same job — but a half-hardened parser
reads as a hardened one.

**"The staging step self-validates" was true on two platforms of three.** Linux reads ELF section
headers and asserts the stripped `.so` still exports `GetPluginFactory`; macOS asserts both slices
are present. **Windows re-lists the extensions the purge just deleted** — it can only fire if
`Remove-Item` silently failed, so it is a delete-confirmation, not a property check, and nothing
asserts the shipped `.vst3` is still loadable. `CI_CD.md` now carries a per-platform table instead
of one sentence covering all three, and the gap is a `TODO(P1)` in the workflow. Recorded with it:
the honest closure is the PE export table, **not** a byte-string search for the symbol name, which
would prove only that the name appears somewhere in the file — the same "looks like a check, checks
nothing" shape as the `lipo -archs` finding in the fourth pass.

**CodeQL `actions` coverage is narrower than its rationale implied.** The comment argues workflow
scanning must stay on through P0 "exactly the phase in which these workflow files are being
written", but both triggers are `branches: [main]`: a workflow change is analysed on the PR into
`main` and on the post-merge push, never on a direct feature-branch push. Nothing reaches `main`
unscanned — the property that actually matters — but the scan is not continuous during iteration.
Stated in the workflow rather than left to be rediscovered.

**Confirmations (all previously recorded, re-reported unchanged):** the `GIT_SHALLOW` + commit-SHA
trap, the reusable-workflow caller-event hazard, and CodeQL's `paths-ignore` being an alert filter.

### Closing note on the review cycle

Ten passes. The defects that mattered were found by **executing** something — pluginval's seed
sentinel, `build.sh`'s exit status, the `lipo` non-assertion, the folded-scalar flag loss — and the
last three passes returned only documentation-consistency findings, because there is nothing left in
this repository to execute: no `src/`, no `tests/`, no `CMakeLists.txt`. Further review of the
scaffolding is negative-yield; the remaining risk lives entirely in code that does not exist yet.
The P0→P1 gate is `DEVELOPMENT_BRIEF.md` §11/§24 (owner sign-off on `DESIGN.md`) plus the three
`Blocking P1` entries in `OPEN_QUESTIONS.md` — decisions, not findings.

Prior: for the **ninth review pass** (2026-07-30). Four findings fixed, four
confirmations — two of them closed with live evidence from the repository's own CI rather than
reasoning.

**`CI_CD.md` still described the pre-per-platform pluginval guard.** The eighth pass re-synced
`DEVELOPMENT_BRIEF` §19.1 and left behind the procedure a developer actually opens: it named only
the randomise step and omitted Linux's `steps.strip.outcome` term — the one term that stops the
gate validating a partially-stripped binary. Replaced with a per-job table of the *actual*
conditions plus why the Linux term is load-bearing, so weakening it requires overruling a stated
reason rather than deleting an unexplained clause.

**Two stale section pointers, found by sweeping rather than by report.** `DSP_POLICY` invariant 11
cited `TESTING_POLICY.md §20.4`, which does not exist — `TESTING_POLICY` has no numbered sections;
§20.4 belongs to `DEVELOPMENT_BRIEF`. The review caught that one. Sweeping every `FILE.md §N`
reference in the repository against the target's actual headings caught a second the review missed:
`FUTURE_RISKS` RISK-005 cited `RELEASE_POLICY.md §8`, also unnumbered. Both now point at heading
names, which do not renumber.

**The debug-directory comment stated the wrong invariant.** It warned against "a rename to a PREFIX
relationship", but `dist/Anabasis-Linux` *is* already a string prefix of `dist/Anabasis-Linux-debug`
— the comment described the current layout as avoiding something it does not avoid. The real
invariant is **path ancestry**: `upload-artifact` and `find` both treat the path as a literal
directory, so a shared prefix is harmless and only *nesting* would leak symbols. Restated.

**`dependency-review.yml` closed with evidence, not analysis.** The review reasoned it could fail
on every PR, since the action needs the dependency graph and a *private* repo would additionally
need GHAS, while the product is described as closed-source. Checked the repository instead: it is
**public**, and the `dependency-review` check on PR #1 is **green**. The fork-PR half of the
finding is real but already handled — `pull-requests: write` is not grantable to fork PRs, so
`comment-summary-in-pr: on-failure` simply cannot comment there; results still appear in Checks,
which is where the gate lives.

*Separately, and not a code matter:* the repository being public contradicts the "closed-source
commercial software" description in `README.md` and `bug_report.yml`. Flagged to the owner; not
changed from here, in either direction.

**Confirmations:** the pre-P1 CI state was verified live rather than predicted — `preflight`
succeeds, all three build jobs skip, `Analyze (actions)` succeeds and no `Analyze (c-cpp)` check is
created, exactly as the dynamic matrix and the branch-protection note in `CI_CD.md` describe. The
`GIT_SHALLOW` + SHA trap, the reusable-workflow caller-event hazard, the CodeQL `paths-ignore`
scope and the macOS `set -e` / dSYM interaction were all re-reported by the review and are
unchanged from the passes that recorded them.

Prior: for the **eighth review pass** (2026-07-30). Six findings fixed, five
confirmations — two of them upgraded from `Unverified` to `Verified` by running the actual tool.

**The "deterministic" pluginval mode was never deterministic.** Both scripts passed
`--random-seed 0`, and **0 is pluginval's sentinel for "generate a random seed"** — `PluginTests.h`
states it outright ("the seed to use for the tests, 0 signifies a randomly generated seed") and
`CommandLine.cpp` only forwards the flag to the validator when it differs from that default, so
passing 0 was byte-for-byte equivalent to passing nothing. The release gate's mode A therefore drew
a fresh seed on every run while the policy, the brief and the procedure all called it reproducible;
a seed-dependent failure would have been unreproducible from the CI log. Confirmed against
pluginval 1.0.4 before and after: `--random-seed 0` printed a different `Random seed:` on each of
three runs, `--random-seed 1` printed `0x1` every time, and the fixed script now prints `0x1`
twice while randomise mode still varies. Both scripts pin a nonzero constant, documented as
load-bearing in each. *Nothing enforces that the two constants stay equal* — each script's comment
names the other, which is the entire mechanism; stated as such rather than dressed up as a check.

*The review raised this differently* — that `--random-seed` is a no-op without `--randomise`. That
premise is wrong, and checking it is what surfaced the real defect: the seed feeds the RNG the
tests themselves draw from (`Validator.cpp` hands it to `UnitTestRunner::runTests`), while
`--randomise` only shuffles test order. The flags are independent; the value 0 was the bug.

**`scripts/build.sh` reported failure after a successful build.** The last statement was
`[ -n "$STATE_TESTS" ] && echo …`; `set -e` does not abort on the left of an `&&` list, but the
last command's status *is* the script's status, so a build without the state-test binary exited 1.
`docs/procedures/DEVELOPMENT.md` documents `scripts/build.sh Debug && scripts/run-tests.sh`, which
would then never have run the tests — and it fails *silently*, since the build genuinely succeeded.
Reproduced (exit 1 with the artefacts absent, exit 0 only when the last one happened to be
present), converted to `if … fi`, and re-verified: exit 0 with none, some, and all artefacts.

**The Windows retry rationale was invented (C7).** `TESTING_POLICY` rule 3 and `TESTING.md` both
said "pluginval returns its assertion count directly", which is why 128…255 counts as a real
failure there. It does not: `CommandLine.cpp` routes every failure through `exitWithError`, which
sets the return value to **1** — the count goes to the log text only. The *conclusion* survives
(Windows has no signals, so nothing in 1…255 can be a crash) but the reason is now the evidenced
one. Also recorded: a malformed argument exits `-1` (255 on POSIX), which both scripts misclassify
as an abnormal termination and retry three times — harmless, since only a broken script can produce
it, but it is the one code neither classifier gets right.

**Action refs verified rather than asserted.** The previous audit said the pinned majors were
"re-aligned to the versions the sibling repository runs green", which this repository cannot
substantiate. All seven `uses:` refs were resolved against GitHub: all exist. One is not what its
spelling suggests — **`actions/dependency-review-action@v5` resolves through a branch**
(`refs/heads/v5`); the tags run `v4.9.0` → `v5.0.0` with no bare `v5` tag. It works and is the
vendor's advertised usage, but it is strictly less immutable than a tag, on the one workflow *not*
gated behind `preflight` and therefore running on every PR today. Recorded in `DEPENDENCY_POLICY`
with the deliberate trade behind floating majors.

**`DEVELOPMENT_BRIEF` §19.1 still described the pre-per-platform randomise guard** — it named only
the randomise step and omitted Linux's `steps.strip.outcome` term added last pass. §20.5 rule 3 had
the matching platform-neutral drift (`exit < 128`). Both re-synced to the workflow.

**Two comments added where the correctness is load-bearing but invisible:** that
`dist/Anabasis-Linux-debug` and `dist/Anabasis-Linux` must stay *siblings* (a rename to a prefix
relationship would put the symbols inside the uploaded tree and the scanned tree at once), and that
CodeQL's `paths-ignore: build` only keeps JUCE out of the results while `FETCHCONTENT_BASE_DIR`
stays at its default under `-B build` — to re-verify when `CMakeLists.txt` lands.

**Confirmations:** the PE/CodeView offsets, the macOS `set -e` / best-effort-dSYM interaction,
`run-tests.sh`'s fail-closed discovery, the reusable-workflow caller-event hazard, and the
`GIT_SHALLOW` + SHA trap (recorded last pass) all re-checked with no change needed.

Prior: for the **seventh review pass** (2026-07-30). Four findings fixed, five
confirmations.

**The Linux randomise gate could validate bytes nobody ships.** Both pluginval steps keyed on
`steps.build.outcome == 'success'`, which stays `success` when the Linux **strip** step fails — so
`build.yml`'s header claim that "on Linux the gate validates exactly the stripped bytes users
receive" was false in precisely the case that matters. Both steps now carry the same explicit
per-platform condition, Linux additionally requiring `steps.strip.outcome == 'success'`; the earlier
asymmetry (deterministic carried no `if:` and self-skipped, randomise carried one) is gone, so the
two modes still report independently but neither runs against a binary in an unshippable state.
Verified by parsing the workflow: Linux both modes identical and requiring strip, Windows/macOS both
modes identical and not.

**`TESTING.md` stated the pluginval retry boundary platform-neutrally**, the same defect corrected
in `TESTING_POLICY` rule 3 last pass — the procedure a developer actually reads still said
`exit ≥ 128` is a crash everywhere, while `run-pluginval.ps1` treats 128…255 on Windows as a *real*
failure. Replaced with the per-platform table and the reason (Windows has no signals; pluginval
returns its assertion count directly — **that second clause was wrong; see the eighth pass above,
which replaces it with the evidenced reason**). A policy and the procedure describing it diverging
is the same class of drift as a policy and its script.

**Requiring the CodeQL check by name would block every pre-P1 PR.** The dynamic matrix means the
`Analyze (c-cpp)` check *does not exist* until `CMakeLists.txt` lands — a required check that is
never created never reports, so the PR waits forever. This is a second, distinct branch-protection
trap from the `paths-ignore` one already recorded (that one is about docs-only PRs; this one is
about the phase). Written into `CI_CD.md` as its own item: require `Analyze (actions)` now,
`Analyze (c-cpp)` only from P1.

**`GIT_SHALLOW` + a commit SHA is a documented CMake trap**, recorded in `DEPENDENCY_POLICY` to be
verified when `CMakeLists.txt` lands. CMake's own documentation says `GIT_SHALLOW` expects `GIT_TAG`
to name a branch or tag; fetching an arbitrary SHA shallowly works only where the server permits
`uploadpack.allowReachableSHA1InWant`. Failure is either a silent full clone (slow, not wrong) or a
hard configure error against a stricter mirror. The resolution is stated in advance so nobody
"fixes" it the wrong way: **drop `GIT_SHALLOW`, never the SHA** — the SHA is what makes the pin
immutable.

**Confirmations:** the PE/CodeView offsets, the macOS `set -e` / best-effort-dSYM interaction,
CodeQL's `paths-ignore` as an alert filter, `run-tests.sh`'s fail-closed discovery, and the
reusable-workflow caller-event hazard (already written into `build.yml` and `CI_CD.md`) were all
re-checked with no change needed.

Prior: for the **sixth review pass** (2026-07-30). Seven findings fixed, four
confirmations.

**`HANDOVER` understated the P1 blockers — the one drift that could actually mislead someone.** It
said "One item blocks P1: owner sign-off on `DESIGN.md`", while `OPEN_QUESTIONS` tags **three**
entries `Blocking P1`: that sign-off plus **OQ-010** (lookahead 0/off position — must be settled
before the parameter exists, since widening a range later re-scales saved sessions) and **OQ-011**
(macOS deployment target, carrying a `TODO(P1)` in `build.yml`). Someone resuming from the
designated status snapshot would have started P1 believing it was clear. Corrected, with a standing
instruction that the row must agree with every `Blocking P1` entry in `OPEN_QUESTIONS`.

**`DSP_POLICY` invariants 2 and 3 could not both hold as written.** Invariant 2 asserted that with
oversampling off the reported latency is "exactly the engaged lookahead, and nothing else
contributes", while invariant 3 requires true-peak detection at **≥ 4× regardless of the user
setting**. If that ≥ 4× path sits in the signal path (linear-phase/polyphase in particular) it
contributes its own delay and invariant 2 is simply false. The two readings — measurement tap vs
in-path — are now stated explicitly as an **open point the P0 oversampling/latency ADR must
settle**, with the guarding test to be written against whichever is chosen rather than against the
convenient one. Invariant 5's "oversampling off ⇒ no oversampling latency" is cross-referenced,
since it rests on the same assumption.

**`TESTING_POLICY` rule 3 stated the retry boundary platform-neutrally** (`exit ≥ 128` is a crash)
while `run-pluginval.ps1` classifies 128…255 as a *real* failure and only ≥ 256 / negative / absent
as an abnormal termination — correct for Windows, which has no signals, but the binding policy did
not carve it out. Now stated per platform, because this repository's own rule is that a script and
the policy describing it never diverge silently.

**Two PowerShell discovery sites threw instead of diagnosing.** `Get-OneTestExe` and the Windows
staging locates run `Get-ChildItem` under `$ErrorActionPreference = 'Stop'` with no
`-ErrorAction SilentlyContinue`, so a missing `build/` produces a .NET stack trace instead of the
"build first" message written right below. Same defect fixed in `run-pluginval.ps1` last pass;
fixed here for consistency, which is the point — two discovery sites behaving differently is what
invites the next bug.

**`CI_CD.md` gave an inaccurate reason for `msvc.yml` being inert.** It said the path filters mean
"push/PR events cannot start it" — but `.github/workflows/msvc.yml` is *itself* in the filter list,
so a change to that workflow on `main` does start it; what makes it a no-op is `preflight`. The
conclusion held, the reasoning did not.

**The ambiguity diagnostics mangled paths containing spaces.** `printf '  %s\n' $matches` relied on
word splitting; quoting it would have indented only the first line. Both scripts now read line by
line, verified against a build tree with a space in a directory name.

**Recorded, not changed:** inside a reusable workflow `github.event_name` reflects the **caller's**
event, so a future `workflow_call` caller invoked on a same-repo `pull_request` would skip
`preflight` and get **zero** build jobs while still reporting success. The planned caller
(`release.yml`, tag-push-triggered) is unaffected, so this is latent — noted in `build.yml` and
`CI_CD.md` to be re-read when `release.yml` lands at P6.

**Confirmations:** the PE/CodeView offsets, the randomise-gate and checkpoint behaviour, the macOS
`set -e` / best-effort-dSYM interaction, and CodeQL's `paths-ignore` as an alert filter were all
independently re-verified with no change needed.

Prior: for the **fifth review pass** (2026-07-30). Three findings fixed, the rest
confirmations or repeats against an earlier revision.

**The per-entry CodeQL gate added last pass did not work.** It used a job-level
`if: matrix.language != 'c-cpp' || …`, but **`matrix` is not an available context in
`jobs.<id>.if`** (only `github`, `needs`, `vars`, `inputs` are). The expression either fails
workflow validation or evaluates empty — making the test always true, so the `c-cpp` entry would
have run regardless and failed on the missing project: precisely the red-CI noise the guard exists
to prevent, and the previous entry claimed it was fixed. Replaced with the mechanism that actually
holds: `preflight` **emits the matrix as JSON**, `analyze` consumes it through
`strategy.matrix: ${{ fromJSON(needs.preflight.outputs.matrix) }}` (where `needs` *is* available),
and the job carries no `if:` at all. Both matrix shapes were validated as JSON.

**`run-pluginval.ps1` was the last discovery site still taking the first match**, on the one
platform where it matters most — `windows-latest` uses the multi-config Visual Studio generator, so
several configurations of `Anabasis.vst3` genuinely coexist in one tree and the release gate could
have passed on a Debug or leftover bundle. Now exactly one match or fail, matching `run-tests.sh`,
`run-pluginval.sh` and the Windows staging step. A second defect in the same lines: with
`$ErrorActionPreference = 'Stop'`, a missing `build/` made `Get-ChildItem` **throw**, so the
intended "build first" message was unreachable and the operator got a stack trace instead of the
one-line fix — fixed with `-ErrorAction SilentlyContinue`.

**The Linux debug-leak check had a dead predicate.** `find … -type f \( … -o -name '*.dSYM' \)`
can never match: a `.dSYM` is a **directory**. Harmless on Linux (no dSYMs are produced there), but
the macOS step calls this "parity with the Linux/Windows staging self-checks" while correctly
omitting `-type f`, so the Linux check read stronger than it was. Aligned, and confirmed by test
that the predicate now matches a `.dSYM` directory.

**Repeats already handled, re-confirmed here:** the merge-commit / merge-queue nuance of the
same-repo PR skip and the `msvc.yml` double-no-op rehearsal note were both written into `CI_CD.md`
in the previous pass; the deployment target (OQ-011) and the CodeQL `paths-ignore` required-check
trap remain tracked and unchanged. Independently verified again with no change needed: the
PE/CodeView offsets, the randomise-gate and `public_ok`/`debug_artifacts` checkpoint behaviour, the
macOS `set -e` / best-effort-dSYM interaction, CodeQL's `paths-ignore` being an alert filter, and
`run-tests.sh`'s fail-closed discovery.

Prior: for the **fourth review pass** (2026-07-30). Six findings fixed, six were
confirmations or already-documented repeats.

**The macOS universality "check" checked nothing.** The packaging step *printed* `lipo -archs` for
each bundle. `lipo -archs` exits 0 for any valid Mach-O including a thin one, so a single-slice
build would have been packaged, uploaded and labelled universal — Intel users would download a
plug-in that cannot load, with CI green. It is now an assertion: both `arm64` and `x86_64` must be
present or the job fails. This is the exact failure mode the folded-scalar regression above would
have produced had it hit `-DCMAKE_OSX_ARCHITECTURES`, so the two findings are the same story twice.

**A developer-side symbol problem could withhold the Windows customer artifact.** The upload gated
on the whole staging step succeeding, and PDB retention lived inside it — so a missing CodeView
record or a non-unique PDB blocked the beta artifact even though the public copy was already
assembled, purged and clean. macOS treats that class of failure as best-effort, so the platforms
had opposite policies. The staging step now emits `public_ok=true` immediately after the public
copy passes its leak check and *before* the PDB work; the customer upload gates on that checkpoint,
the `-debug` upload on the later one. PDB retention stays strict on purpose — on macOS a missing
dSYM is expected under Release+LTO, whereas `/DEBUG` guarantees a PDB — but its strictness now
costs only the artifact it protects.

**Workflow security scanning was switched off for all of P0.** `codeql.yml`'s `preflight` gated the
whole matrix, including the `actions` entry, which needs no build and analyses the very workflow
files being written this phase. The gate is now per matrix *entry*: `actions` always runs, `c-cpp`
waits for `CMakeLists.txt`.

**Windows self-test discovery took the first match.** `Select-Object -First 1` against a
multi-config generator could run a Debug binary while the artifacts come from Release. Now exactly
one match or fail — matching the staging step in the same job and `run-tests.sh`. The same
`head -n1` pattern in `run-pluginval.sh` is fixed the same way (verified across none / one / two
matches), so the release gate cannot validate a different bundle than the one just built.

**This audit claimed a checker that does not exist (C7).** The previous entry said "a checker
asserts every configure flag survives comment-stripping on all three platforms". No such checker is
in any workflow or script — the flag list was verified *by hand* during that fix and nothing re-runs
it. Corrected to say so. Inventing a safety net is worse than having none, because it stops anyone
writing the real one.

**`CI_CD.md` documented the old randomise guard** (`!cancelled()` only) after the workflow gained
`&& steps.build.outcome == 'success'`; the summary in `DEVELOPMENT_BRIEF` §19.1 had the same drift.

**Confirmations and already-handled repeats:** the PE/CodeView offsets were independently
re-verified against the spec; the macOS `set -e` / best-effort-dSYM interaction was traced and is
sound; CodeQL's `paths-ignore` is an alert filter as its comment says; `run-tests.sh`'s fail-closed
discovery was re-confirmed. Newly recorded in `CI_CD.md` rather than changed: the same-repo PR skip
means the **push** build validates the branch head while a PR-event build would have validated the
*merge commit* — a real gap only once a merge queue or "require branches to be up to date" is
enabled; and `msvc.yml` is doubly inert until P1, so its pinned third-party action should be
rehearsed via `workflow_dispatch` at P1 rather than first executing inside the P1 build PR.
`CMAKE_OSX_DEPLOYMENT_TARGET` (OQ-011) and the CodeQL `paths-ignore` / required-check trap are
unchanged and already tracked.

Prior: for the **third review pass** (2026-07-30). Four findings fixed, three were
confirmations or verified non-issues.

**A regression introduced by the previous pass — the macOS build silently lost two flags.** The
`TODO(P1, OQ-011)` comment added last pass sat *inside* a `run: >` folded block scalar. YAML joins
every line of a folded block into one line and gives `#` no meaning, so bash saw a single command
with a trailing comment and dropped everything after it — `-DCMAKE_OSX_DEPLOYMENT_TARGET` **and**
`-DANABASIS_BUILD_NUMBER`. Consequence once P1 lands: macOS builds would carry an unintended
minimum-OS setting and report **build number 0** in the About box, which the bug-report form asks
testers to quote. No error would have revealed it. The comment now lives above the step, says why
it must stay there. **No automated checker guards this** — the flag list was verified by hand
during that fix (parsing the workflow, stripping at the first `#` the way bash would, and
confirming every configure flag survives on all three platforms) and nothing re-runs that check;
an earlier revision of this paragraph claimed a checker exists, which was an invented facility
(constraint C7). If it is worth guarding, it needs writing.

**The randomise pluginval step ran after a failed build.** Its `if: ${{ !cancelled() }}` forced it
to run when the build had failed, producing a second red step complaining about a missing plugin —
exactly the noise the staging/packaging guards were fixed for in the previous pass. Now
`!cancelled() && steps.build.outcome == 'success'` on all three jobs (the Linux build step gained
the `id` it needed); the deterministic step has no `if:` and already skipped itself correctly. The
"both modes report independently" intent is unchanged.

**`FUTURE_RISKS` RISK-003 cited `TESTING_POLICY` rule 4** for the hostile-input requirement, which
the previous pass's renumbering moved to rule **5** (rule 4 is now the skipped-test-category rule).
Swept the repository for other stale rule pointers — the two remaining (`POSTMORTEMS`, `TESTING.md`,
both rule 1) are correct.

**Verified, no change:** the issue-form / `config.yml` URLs use `skyRolly/Anabasis`, which matches
both this repository's git remote and the sibling product's own links — the casing is the canonical
display slug, not a mismatch. CodeQL's `paths-ignore` is an alert filter, which is exactly what its
inline comment claims. The `run-tests.sh` fail-closed discovery was re-confirmed behaviourally.
The branch-protection interactions (`build.yml` same-repo PR skip, `codeql.yml` `paths-ignore`)
were already documented in `CI_CD.md` §"Before enabling branch protection" in the previous pass.

Prior: for the **second review pass** (2026-07-30). Eleven findings addressed.

**The overview contradicted the decisions.** README §Project status still listed the JUCE pin and
the plugin identity as open items "not to be guessed at" after the same change set resolved and
froze both, and the `DEVELOPMENT_BRIEF` §23 delta table still said the JUCE tag "must be checked at
P0" — contradicted three sections later by §23.2. README now separates *decided and frozen* from
*still open*, and §23 gained a **Plugin identity** row so the delta table records the real
difference (`RTec`/`Anbs`/`com.rollytech.anabasis`, VST3 categories Fx/Dynamics/Mastering) rather
than a stale instruction.

**CI correctness.** The Windows staging and macOS packaging steps ran on `!cancelled()` alone, so a
failed *build* let them run and fail a second time on missing paths — a red step unrelated to the
cause; both now also require `steps.build.outcome == 'success'` (the Linux job needs no guard, its
strip step has no `if:`). `objcopy --add-gnu-debuglink` recorded a CI-workspace path that exists on
no user's machine, so a downloaded `-debug` artifact would never be found automatically; objcopy now
runs from inside the debug dir with a bare basename, the conventional lookup. The hand-rolled PE
CodeView parser now bounds-checks `PointerToRawData` before indexing, so a truncated image produces
the intended diagnosable error instead of a raw .NET exception.

**Honesty about what the gate covers.** The `build.yml` header claimed the release gate validates
the shipped bytes; that is true only on **Linux**, where the strip precedes pluginval. macOS and
Windows strip (and macOS signs) *after* validation, so a defect introduced by stripping or signing
would ship unvalidated there. Stated plainly in the header and in `CI_CD.md`, and raised as
**OQ-012** for P6 — reordering is not free (macOS must codesign last) and deserves a measurement,
not a guess.

**Branch-protection traps documented before they bite.** `CI_CD.md` gained a "before enabling
branch protection" section: `build.yml`'s same-repo PR skip reports a *skipped* conclusion (fine,
but the first thing to check if a required check ever hangs), and — the sharper one — `codeql.yml`'s
`paths-ignore` means the workflow is **never created** for docs-only PRs, so a required CodeQL check
would block them forever. Neither is a bug; both are traps if configured blindly. The standard
same-job-names no-op companion workflow is named as the fix, to be added when CodeQL is made
required and not before.

**Scripts.** `run-tests.sh` now fails closed on **ambiguity** as well as absence: `find | head -n1`
would silently gate on whichever binary `find` emitted first, so a stale second build tree could
produce a green report about the wrong artifact. Verified in all three cases (none / exactly one /
two). `run-pluginval.sh`'s `chmod +x … || true` no longer swallows a setup failure that would
otherwise resurface as an opaque "cannot execute" from the validation loop.

**Also:** `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` was inherited unexamined — arm64 macOS starts at 11.0,
so the arm64 slice's minimum is silently raised, and 10.13 may sit below JUCE 9's floor. Carrying a
`TODO(P1)` and raised as **OQ-011**; not guessed at, since the deployment target is a user-visible
support claim (C7). `TESTING_POLICY`'s rule "3a" did not render as a list item (Markdown does not
recognise `3a.`), so the skipped-test-category rule — cross-referenced from the coverage audit — is
renumbered to **rule 4**, with the hostile-inputs rule to 5.

Prior: for the **first review pass** (2026-07-30). Ten findings fixed. Corrected in that
audit: the policies row said 15 docs, the tree has **16**. Scripts: `setup-linux.sh` now installs
`curl` + `unzip` (`run-pluginval.sh` calls both; `libcurl4-openssl-dev` is headers, not the CLI, and
GitHub runners preinstall them — so the gap only ever showed on a fresh machine);
`run-pluginval.ps1` **no longer passes `--skip-gui-tests`**, which was inherited from the sibling
product where an evidenced runner limitation justifies it, and here suppressed nothing while
contradicting the "uniform and blocking on every platform" gate — `TESTING_POLICY` gains rule 4
requiring any future skip to be documented, not merely scripted; `build.sh` `find` calls take
`-maxdepth` before `-name`. CI: all actions re-aligned to the versions the sibling repository runs
green (`checkout@v7`, `upload-artifact@v7`, `codeql-action@v4`, `dependency-review-action@v5`) —
the scaffold had them a major version behind; `preflight` now skips same-repo `pull_request` events
that `push: ["**"]` already built; the Linux/Windows debug uploads gate on a `debug_artifacts`
output written last rather than on "not skipped", so an aborted symbol step cannot produce a second
misleading `if-no-files-found` failure. Docs: `CI_CD.md`'s pipeline list claimed self-tests before
symbol handling — on Linux the strip runs **first**, deliberately, so the gate validates the
shipped bytes; the list is now per-platform. `DSP_POLICY` invariant 2 and the release checklist
asserted "with lookahead 0 … reported latency is 0", which §4.3's 0.5–10 ms range makes unreachable
— rephrased against the engaged lookahead, with the underlying question raised as **OQ-010**
(does lookahead get an explicit off position? — it must be settled before the parameter exists,
since widening a range later is compatibility-gated). `bug_report.yml` uses an absolute doc URL
(a relative one does not resolve from `/issues/new`).

Prior: for the **OQ-001 / OQ-003 resolutions** (2026-07-30). Two blocking decisions
moved to `Resolved` in `OPEN_QUESTIONS.md` (entries are never deleted): the JUCE pin is **9.0.0 at
commit `f8f8864…`**, the same revision Anamorph pins, so the product line shares one framework
baseline; and the plugin identity is **`RTec` / `Anbs` / `com.rollytech.anabasis`**, with the
vendor code spelling RollyTech rather than the first product. Synced: README (§Requirements),
`DEPENDENCY_POLICY` (pin row + the shared-pin rationale), `BUILD.md` (toolchain, options table,
new §Plugin identity), `HANDOVER` (dependency row, Known Blockers — now one, `DESIGN.md`
sign-off), `DEVELOPMENT_BRIEF` §23.2. Both values must be written into `CMakeLists.txt` at P1 and
are frozen from the first build that leaves this repository. Anabasis pays nothing for the
identity decision because it has never built; the sibling product absorbs the one-time break
(Anamorph 0.9.1 / its ADR-0023 / its KI-016). No `src/` change — there is still no `src/`.

Prior: repository bootstrap — the migration of Anamorph's governance system,
documentation library, build/CI scaffolding and working conventions into a previously empty
Anabasis repository, plus the product brief (`docs/DEVELOPMENT_BRIEF.md`, Part I unchanged from
the owner-supplied prompt + an additive Part II recording the inherited engineering standard).
No `src/`, no `tests/`, no `CMakeLists.txt` — so every claim about runtime behaviour in this
repository is `Unverified` **by construction**, and the policies state invariants the future code
must satisfy rather than compliance it already has (constraint C7).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| *(none — `src/` does not exist)* | — | — | — |

Rows are added as modules land. The planned module set and its responsibilities are listed in
`docs/REPOSITORY_MAP.md` §`src/`; that is a **plan**, not coverage.

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | DEVELOPMENT_BRIEF, **DESIGN**, SOURCE_OF_TRUTH, REPOSITORY_MAP, OPEN_QUESTIONS, HANDOVER, DOCUMENTATION_COVERAGE, KNOWN_ISSUES, FUTURE_RISKS, POSTMORTEMS, BRAND_CONSISTENCY_CHECKLIST | Present (`DESIGN.md` is **`Accepted`**, signed off 2026-07-31; it ranks at level 5 and is superseded section by section by its ADRs — SOURCE_OF_TRUTH §"Where `DESIGN.md` sits") |
| worklogs | `2026-07-30-p0-anamorph-research.md` | Present (raw evidence trail; never cited as policy) |
| policies | 16 docs (incl. the Anabasis-specific `MODE_AND_ADAPTATION_POLICY`) | Present |
| procedures | BUILD, DEVELOPMENT, CI_CD, TESTING, RELEASE_PROCESS, RELEASE_COMPATIBILITY_CHECKLIST, TROUBLESHOOTING | Present (PACKAGING deferred to P6) |
| architecture | `design-decisions/ADR_INDEX.md` + **ADR-0001…0011 (Accepted 2026-07-31)** | Decisions complete for P0; the *descriptive* set (ARCHITECTURE, LATENCY_MODEL, PARAMETER_REGISTRY, …) lands with P1–P2 |
| user | — | Deferred to P6 |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | — | Deferred to P6 (produced against a real dependency tree; copying another project's inventory would be invented evidence) |
| root — internal/testing | — | Deferred to P6 (SUPPORT.md ships with the first tester build) |
| .github | workflows/{build,codeql,msvc,dependency-review}.yml, dependabot.yml, ISSUE_TEMPLATE/{bug_report,config}.yml | Present (release.yml deferred to P6) |
| scripts | setup-linux, build, run-tests, run-pluginval.{sh,ps1} | Present |

## Known coverage gaps / TODOs

These are **deliberate**, not oversights. Each names what would close it.

- **No architecture set** — `ARCHITECTURE.md`, `SIGNAL_FLOW.md`, `DSP_GRAPH_REFERENCE.md`,
  `THREAD_MODEL.md`, `PARAMETER_REGISTRY.md`, `SERIALIZATION_REGISTRY.md`, `LATENCY_MODEL.md`,
  `REALTIME_SAFETY_AUDIT.md`, `COMPATIBILITY_MATRIX.md`, `DSP_ALGORITHMS.md`,
  `PERFORMANCE_BUDGET.md` all describe code that does not exist. Closed by P1–P2.
- ~~**No ADRs**~~ — **closed 2026-07-31**: ADR-0001…0011 are Accepted and registered. They remain
  `Unverified` in confidence (no `src/`), which is a *different* gap from absence: each is a
  contract the P1+ code must satisfy, and its confidence is upgraded as its code and tests land.
  New decisions still follow C1 — evidence-driven, no quota.
- **Policy compliance sections are `TODO (no code yet)`** in `REALTIME_AUDIO_POLICY`,
  `THREADING_POLICY`, `DSP_POLICY` and `MODE_AND_ADAPTATION_POLICY`. Closed as each phase lands,
  with evidence citations.
- **No performance or aliasing numbers anywhere** — and none may be written until measured with a
  recorded machine and methodology (constraint C2). Closed by `TEST_REPORT.md` at P2/P6.
- **No host (DAW) matrix** — requires manual testing. Closed by the P6 DAW smoke tests.
- **Legal / attribution class absent** — closed at P6 against the actually-pinned JUCE tree.
- **`docs/user/` absent** — closed at P6.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD (or the change description before
the first tag) and adjust the affected rows. A new module → add a row; a new doc → add to
self-coverage; new measured data → upgrade the confidence. Never upgrade a confidence level
without the evidence that justifies it.
