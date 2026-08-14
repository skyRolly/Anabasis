# gate-reverts/ — the two ⊕ unratified changes, as ready-to-apply patches

**These patches are NOT applied.** They exist so that "split the gated changes out" is a command
rather than a project, and so the owner's two options — ratify, or remove — cost the same.

`ARCHITECTURE_REVIEW_GATE.md` says a gated change "must NOT be auto-merged even if CI, the
self-tests, and pluginval all pass". 0.1.4 carries two such changes, both disclosed and both
unratified:

| Patch | ADR | What it removes | Suite after applying |
|---|---|---|---|
| `gate1-slot-payload-read-rules.patch` | [ADR-0026](../../architecture/design-decisions/ADR-0026-slot-payload-read-rules.md) | the two `SLOT` read-rule expressions | 839 checks, **5 failures** |
| `gate2-painting-thread-read.patch` | [ADR-0027](../../architecture/design-decisions/ADR-0027-painting-thread-reads-editor-bookkeeping.md) | the editor→painting-thread wiring | 839 checks, **0 failures** |

Both figures are **measured**, not estimated (2026-08-13, this tree).

## The asymmetry is the useful part

**Gate 1 cannot be reverted silently.** Five assertions fail, three of them naming the defect the
change closed (`a payload-less slot lent its preset name to another state ('Ghost Session')`). The
suite is the specification of what reverting gives up. A partial revert is also available and
measured — keeping decision 1 and dropping decision 2 fails 2 — because the two decisions are
independent; ADR-0026 tabulates all three states.

**Gate 2 reverts with nothing failing.** That is not evidence it is safe to drop: it is evidence the
path is untested, which ADR-0027 states outright. The cost of applying that patch is a behaviour no
headless suite can see — either a `ResizableBorderComponent` loses the frame the override exists to
protect, or the parented pop-up's doubled edge returns. It has to be judged by eye in a real host.

## Applying

```sh
git apply docs/procedures/gate-reverts/gate1-slot-payload-read-rules.patch
git apply docs/procedures/gate-reverts/gate2-painting-thread-read.patch
```

Each patch touches CODE only. The records stay: an ADR that was declined is still the record of the
decision, and `SESSION_COMPATIBILITY_POLICY.md` / `THREADING_POLICY.md` keep their entries with the
outcome amended. Do not delete the ADRs when applying a patch — mark them Rejected and say why.

## If they are ratified instead

Delete this directory, change each ADR's Status to Accepted with the sign-off quoted in its banner,
drop the ⊕ NOT RATIFIED headers, unmark the `THREADING_POLICY.md` row as pending, and clear the two
⚠ blocks from `HANDOVER.md` and the pull request.
