# Anabasis — Phase 1, first round: what landed, what is next, and what waits for the owner

**Date:** 2026-09-27 · **Version:** 0.2.14 (unreleased) · **Class:** dated record in `docs/reports/` —
a snapshot, superseded by a later record rather than edited in place (`docs/SOURCE_OF_TRUTH.md`).

The durable follow-up record for Phase 1 of the 2026-09-26 audit
([`2026-09-26-anabasis-product-ux-audit.md`](2026-09-26-anabasis-product-ux-audit.md), §Prioritized
roadmap: "Comparisons and session figures tell the truth"). The audit and its finding records are not
edited; this record states, per finding, what this round did and what remains. Phase 0's closure is
[`2026-09-27-phase0-closure.md`](2026-09-27-phase0-closure.md); the evidence for both is
[`worklogs/2026-09-27-phase1-match-statistics-observability.md`](../../worklogs/2026-09-27-phase1-match-statistics-observability.md).

The round was scoped by the owner's instruction to MATCH's signal ordering, the STATISTICS reset and
scope, measurement clarity and graph numeric observability groundwork — no visual polish, and none of
the P2 backlog (resizing, markers, rename/delete, macro redesign).

## 1. Phase 1 findings — status after this round

| Finding (audit priority) | Status | Where |
|---|---|---|
| **UX-009** (P1) MATCH scales BYPASS by the same gain, so the bypass jump stays unmatched | **Done** — the gain moved onto the processed leg; the jump went from −6.76 to +0.63 LU; nothing rendered or metered changed | ADR-0044 (on the owner's direction, ⊕ for review) |
| **UX-002** (P1) the whole STATISTICS panel is an unmarked reset | **Done** — the body is inert; a RESET button on the header line | ADR-0020 amendment 4 |
| **VIS-001** (P2) bypassed passages are folded into the session figures | **Done** — a realtime bypass audition is left out of I, LRA and the TP/SP holds; offline renders measure the file | ADR-0020 amendment 4 |
| **VIS-009** (P2) the session's scope is invisible | **Done, in part** — the header shows how much programme the figures cover, and the manual lists every reset point; the prepare-time reset itself is unchanged (a host re-prepare still starts a new session, now visibly) | ADR-0020 amendment 4 |
| **DOC-002** (P3) a reset blanks the rolling readings too | **Done** — documented (the display clear is kept; the manual now says it) | `USER_MANUAL.md` §3.4 |
| **TEST-001** (P2) the editor tick is untested | **Done** — the whole tick body is driven by 11 tests; 29 of 29 mutants killed | `procedures/TESTING.md` |
| **VIS-007** / **VIS-003** step 1 (Phase 2 in the audit, pulled forward as the groundwork asked for) — no number for the core maximizer reading; GR unattributed | **Done** — "lim GR" (now) and "GR max" (window) in both views, labelled as the limiter's | this round's observability commit |
| **DSP-005** (P2) MATCH's predict floor counts only the limiter | **Next** — measured and recorded, not changed: +0.63 LU at the calibration point, +1.7 LU on hot programme, the **clipper's** loss the larger term (contradicting the audit's "drop the clipper term") | `KNOWN_ISSUES.md` KI-023 |
| **VIS-010** (P2) nothing says the meters read the pre-monitor render with MATCH/DELTA on | **Next** — a label needs a place on a signed surface (brand checklist) and a gain readout needs a new Audio→GUI scalar (a Thread Model gate confirmation) | — |
| **UX-010** (P2) MATCH/DELTA lit but inert offline, with no cue | **In part** — the manual now says both are never in a bounce; the on-screen cue is VIS-010's | `USER_MANUAL.md` §2.4 |
| **VIS-004** (P2) under BYPASS the GR displays keep showing processed-path reduction | **Documented** (manual: the readout keeps showing unheard limiter work under BYPASS); display change not started | `USER_MANUAL.md` §3.4 |
| **VIS-005** (P2) readouts freeze when the host stops calling the plug-in | **In part** — the session length stops and the GR readout's "now" turns "-" after 0.5 s; M/S/RMS/out LUFS still hold their last values | — |
| **VIS-012** (P2) held / live / no-signal not distinguished | **Not started** — needs TEST-004's `formatReading` first | — |
| **VIS-014** (P2) the active integrated standard and RMS reference are not shown on their rows | **Not started** | — |
| **STATE-008** (P2) A/B "independent setups" share the session holds and more | **Not started** — documentation-only in the audit's own recommendation | — |
| **UX-023** (P2) the Standalone's two "Settings" buttons | **Not started** | — |
| **UX-008**, **VIS-013** (P3) | **Not started** | — |

## 2. Recommended order for the next Phase 1 round

1. **DSP-005 (KI-023)** — prototype the predict floor with the clipper's loss and the compressor's
   reduction against the audit's acceptance criteria, inside ADR-0006 decision 7 (stateless,
   floor-only, attenuation-only); the measurement in this round's worklog is the starting point.
   Once BYPASS is at unity this residual is the whole bypass gap.
2. **VIS-010 + UX-010's cue** — decide the monitor-state indicator's place (the top bar beside BYPASS
   is the only undimmed spot) and whether it carries the MATCH gain (a new published scalar).
3. **VIS-005 / VIS-012 after TEST-004** — one `formatReading` rule, with the session length as the
   liveness heartbeat the audit asked for; it now exists.
4. **VIS-014, STATE-008, VIS-004** — documentation or small display changes.
5. **UX-023, UX-008, VIS-013** — last.

## 3. Owner decisions this round did not take

| Decision | Record |
|---|---|
| Confirm the Phase 1 design choices taken on direction: ADR-0044 (where the MATCH gain goes; DELTA + MATCH unchanged) and ADR-0020 amendment 4 (RESET's form and words, the realtime-only bypass pause, the session length) | both flagged ⊕ in their records |
| STATE-002 — widen ADR-0010's lockable set to `{ceiling, truePeakMode}`; the dither rule under a browse | `KNOWN_ISSUES.md` KI-021; the Phase 0 closure record §4 |
| UX-003 — a Save-overwrite prompt: an Anabasis-only or family-wide deviation, the prompt rule, the dialog strings | `KNOWN_ISSUES.md` KI-022 |
| VIS-002 — the STATISTICS TP row's half-print slack in TP mode (an ADR-0020 amendment) | `KNOWN_ISSUES.md` KI-020 |
| ADR-0042 option A (Freeze OFF across a re-prepare) and KI-007 items 1 and 10 | `KNOWN_ISSUES.md` KI-007 |
| The ⊕ voicing constants of the TP clamp (listening) | ADR-0041 |

## 4. Evidence limits carried forward

No listening was performed for any of this round's changes. The realtime MATCH/BYPASS path and the
STATISTICS bypass pause are verified on the engine and the processor, and MATCH/BYPASS also in Carla's
realtime engine (a plug-in host, not a DAW); Ardour's offline exports are byte-identical to the review
round's. Which hosts bypass through the
plug-in's parameter and which stop calling it (the host-dependent half of VIS-001) is the audit's
TEST-002, recorded in the worklog's host section with exactly what was and was not run.
