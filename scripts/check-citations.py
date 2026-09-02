#!/usr/bin/env python3
# ============================================================================
#  check-citations.py — keep `file:line` evidence citations pointing at the code
#  they were written about.
#
#  WHY THIS EXISTS. Several documents of record cite their evidence as
#  `src/PluginProcessor.cpp:695-752`. A line anchor is exact and therefore
#  fragile: any edit ABOVE it silently re-aims it at unrelated code, and the
#  document keeps reading as though it were still correct. The rule the
#  repository already states is that anchors are re-anchored in the SAME change
#  set that moves them — this makes that rule checkable instead of remembered.
#
#  It is worth being precise about what this can and cannot do:
#
#    * It CAN tell you that a citation no longer points at the same TEXT it
#      pointed at in a base revision, and move it back onto that text. That
#      catches the whole class of "an edit above shifted it".
#    * It CANNOT tell you a citation was aimed at the wrong code to begin with.
#      Preserving content identity faithfully preserves a pre-existing mistake —
#      and it does so INVISIBLY, in a DRIFTED line that reads like a repair. Two
#      anchors in this repository were wrong before this file existed
#      (`SERIALIZATION_REGISTRY.md` §1.4's `BASELINE` carriage sites, and
#      `LATENCY_MODEL.md`'s redundant `updateLatency()` call); this tool moved
#      both, correctly, onto the same unrelated code they had always named. Both
#      are now spelled with the FUNCTION beside the line number, which is the
#      only part of a citation a reader can check without the tool. A clean run
#      does not mean every citation is correct. It means none of them MOVED.
#
#  WHAT IT WILL NOT TOUCH, and why that list is the important part. This tool
#  rewrites line numbers, so every citation it misclassifies as ours is a
#  citation it CORRUPTS — it replaces a correct anchor with a wrong one and
#  reports success. It has done exactly that once already (see `classify()`), so
#  the ownership test is now deliberately narrow: a citation is ours only when
#  it names its path from THIS repository's root, with no revision or checkout
#  qualifier in front of it, and that path is one of `TRACKED` verbatim.
#  Everything else — a bare file name, a sibling-product path, a `<rev>:`-pinned
#  anchor — is left alone. Under-checking costs coverage; misclassifying costs
#  the truth of the document, which is the thing being protected.
#
#  PROSE EXAMPLES MUST NOT USE A TRACKED PATH. This tool cannot tell an
#  illustration of a citation from a citation — `DOCUMENTATION_COVERAGE.md`'s
#  own worked example of a substitution bug was silently re-anchored, changing
#  the numbers the sentence depended on. Write examples against a path outside
#  `TRACKED` (`some/file.cpp:107`) and they are left alone.
#
#  Usage:  --check (the default) reports drift · --fix re-anchors it.
#  Exit codes follow the sibling scripts: 0 clean · 1 drift found (--check only)
#  · 2 the run could not reach a trustworthy answer (nothing to check against,
#  or --fix left anchors that need a human).
# ============================================================================

import argparse
import os
import re
import subprocess
import sys

# The sources whose line anchors are worth tracking, spelled EXACTLY as a
# citation must spell them to be checked. A file absent here is not checked; a
# path that differs from the spelling here — including the sibling product's
# `src/PluginEditor.cpp` against our `src/gui/PluginEditor.cpp` — is not ours.
TRACKED = (
    "src/PluginProcessor.cpp",
    "src/PluginProcessor.h",
    "src/gui/PluginEditor.cpp",
    "src/gui/PluginEditor.h",
    "src/gui/LookAndFeel.cpp",
    "src/gui/LookAndFeel.h",
    "src/dsp/AnabasisEngine.cpp",
    "tests/state_tests.cpp",
    "tests/dsp_tests.cpp",
)

# A citation is `<path>:<line>`, `<path>:<start>-<end>`, or a COMPOUND list that
# names the path once and then several anchors: `<path>:708-709, 851, 1208`. The
# trailing group is what an early version of this file missed — it re-anchored
# the first anchor and left the bare numbers behind it untouched, which produced
# `:1040, 1039, 1053` in this repository: out of order and internally
# contradictory, from a tool whose whole job is keeping anchors true.
#
# Two parts of this pattern exist ONLY to stop the tool rewriting somebody
# else's line numbers, and both are load-bearing:
#
#   * The leading lookbehind refuses to start a match in the middle of a token.
#     Without it, `<checkout>:src/PluginProcessor.cpp:485-491` simply matched
#     from `src/…` — the qualifier never reached the ownership test — and the
#     anchor was rewritten by THIS tree's code movement onto lines of another
#     product that contain something else entirely. Rejecting the match is not
#     enough on its own either: the scan would just retry one character later
#     and match `rc/PluginProcessor.cpp`. The lookbehind blocks every one of
#     those restarts, because each is preceded by a path or word character.
#   * `<prefix>` therefore CAPTURES the qualifier — a checkout name, or a pinned
#     revision such as `7686204:` — so `classify()` can see it and decline.
#
# The path must contain a directory separator. A bare `PluginProcessor.cpp:7` is
# ambiguous across checkouts by construction — the architecture documents use a
# bare name as shorthand for "the file I have been quoting", which inside a
# paragraph about another product means that product's file — and no amount of
# context-reading fixes that here. Citations in this repository's own documents
# are spelled from the root, which is what makes them checkable.
CITATION = re.compile(
    r"(?<![\w./\\:-])"
    r"(?:(?P<prefix>[\w.@-]+):)?"
    r"(?P<path>[\w.-]+(?:[/\\][\w.-]+)+):"
    r"(?P<anchors>\d+(?:-\d+)?(?:\s*,\s*\d+(?:-\d+)?)*)")
ANCHOR = re.compile(r"(\d+)(?:-(\d+))?")

# Citations this repository RE-AIMED on purpose: the anchor was pointing at the
# wrong code and was moved onto the right code, which is indistinguishable from
# drift by the base-text test and would otherwise fail the gate forever.
# Declaring one here is a reviewable act — it appears in the diff, beside the
# document it exempts, and it is the only way to make this tool accept a
# re-aim. An entry whose citation no longer differs from the base has done its
# job (the base has caught up) and is reported as removable on the next run.
#
# Spelled `set([...])` rather than `{...}` on purpose: emptying a brace literal
# leaves a DICT, and the set difference below then raises `TypeError` instead of
# reporting a clean run. The list going empty is the expected end state of every
# entry here, so it must be the boring case.
DELIBERATE_REAIMS = set([
    # ROUND 8 (2026-09-02) — nine spellings, fifteen occurrences, and the entry
    # exists because of a PROCESS mistake worth recording rather than a code one.
    #
    # Round 7 inserted 22 lines into `src/PluginProcessor.cpp` (the published
    # channel count and `numChannelsChanged`) and then re-anchored with
    # `--fix` at the script's DEFAULT base, `origin/main`. The gate does not
    # compare against `origin/main` on a push: `build.yml` feeds it
    # `github.event.before`, the branch's previous tip — "one push of drift at a
    # time". So the re-anchor was measured against the wrong revision, the push
    # went red, and the anchors were left naming code 22 lines above their
    # subject: `LATENCY_MODEL.md`'s `updateLatency()` definition pointed at the
    # silent-meter publication, its `prepareToPlay` call pointed at the new
    # channel-count store, and ADR-0026's two SLOT rules pointed into the
    # adaptive restore. Every anchor below was re-derived from the SYMBOL its
    # prose names and verified by reading the line it now points at.
    #
    # THE DECLARATION IS NEEDED BECAUSE THE CORRECTION ITSELF READS AS DRIFT.
    # The gate asks "did this anchor follow the text it named at the base?", and
    # at the base the anchor named the WRONG text; following that faithfully is
    # what the previous rounds did. Moving it onto the right text is a re-aim,
    # which is what this list is for. Each entry is good for exactly one
    # transition — once the base carries the corrected spelling,
    # `whole_base == whole_cur` and `is_declared_reaim` stops matching.
    #
    # The local command that reproduces the gate is
    # `check-citations.py --check --base <previous push tip>`, NOT the default.
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:873-891"),                 # updateLatency(), the definition
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:790"),                     # prepareToPlay's updateLatency() call
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:863-871"),                 # setNonRealtime
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:1922"),                    # the deliberately redundant updateLatency()
    ("docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md",
     "src/PluginProcessor.cpp:1823"),                    # stored.getChildWithName("ANABASIS").isValid()
    ("docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md",
     "src/PluginProcessor.cpp:1842"),                    # live.isValid() && liveSurfaceRestored
    ("docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md",
     "src/PluginProcessor.cpp:1779"),                    # liveSurfaceRestored, the definition
    ("src/PluginProcessor.cpp",
     "src/PluginProcessor.cpp:1558"),                    # applyFactoryPreset's presetBaseline assignment
    ("src/PluginProcessor.cpp",
     "src/PluginProcessor.cpp:1628"),                    # applyPresetFile's presetBaseline assignment

    # 0.1.4 review round 7 — THE WHOLE ARCHITECTURE SET, and the size of this
    # list is the finding. An audit of every tracked citation in the governed
    # documents found that MOST had been aimed at the wrong code since before
    # this tool existed, and every re-anchoring since had faithfully carried
    # each one onto the same wrong text: `LATENCY_MODEL.md` cited the undo
    # stack for `updateLatency()`, `SERIALIZATION_REGISTRY.md` cited the
    # latency predictor for `saveSlotFromLive()`, `THREAD_MODEL.md` cited a
    # comment for the OpenGL context. Every anchor below was re-derived from
    # the SYMBOL its prose names and verified by reading the line, and every
    # one now carries that symbol beside it — the half a reader can check
    # without running anything.
    #
    # Each entry is good for exactly one transition (see `is_declared_reaim`):
    # once `main` carries the corrected spelling, the entry stops matching and
    # the next run reports it as removable. This list is expected to empty.
    ("docs/KNOWN_ISSUES.md",
     "src/PluginProcessor.cpp:195"),
    ("docs/architecture/COMPATIBILITY_MATRIX.md",
     "src/PluginProcessor.cpp:12-14"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:852-870"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:769"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:842-850"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:1901"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "tests/state_tests.cpp:2171"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.h:453"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.h:431-439"),
    # Moved :604 -> :616 on 2026-08-14 when a comment block was added above
    # `PopupShield`. A declaration names the CURRENT spelling, so it has to be
    # carried along by whatever moves that line — otherwise the pair stops
    # matching and the re-aim it excuses reports as fresh drift.
    ("docs/architecture/THREAD_MODEL.md",
     "src/gui/PluginEditor.h:616"),
    # `presetMenusOpen`, moved :330 -> :342 by the same comment block, and this
    # entry is the HALF THAT WAS MISSED when the THREAD_MODEL one above was
    # written in the same commit. The repair itself is what needs declaring: this
    # tool compares TEXT at the base line against text at the current line, so a
    # correct re-anchor and a drift are the same event to it, and the run that
    # follows a repair asks for the repair to be REVERTED (`:342 -> :330`). Fixing
    # an anchor without declaring it therefore turns the gate red on the commit
    # that fixed it — which is exactly what happened, twice in a row, on this
    # branch. Re-anchor and declare in ONE change set, never in two.
    ("docs/architecture/design-decisions/ADR-0027-painting-thread-reads-editor-bookkeeping.md",
     "src/gui/PluginEditor.h:342"),
    # 0.1.5, the JUCE 9.0.1 bump (ADR-0028): `LATENCY_MODEL.md`'s
    # `effectiveFactor()` anchor moved :93-96 -> :96-99 when the OS-latency
    # table's provenance comment above it gained three lines. NO entry was added,
    # and the reason is the one thing this list must not get wrong:
    # `src/dsp/Latency.h` IS NOT IN `TRACKED`, so `classify()` declines the
    # citation and the tool never looks at it — not checked, not re-anchored, not
    # excusable. The :96-99 edit was made and verified BY HAND. Both `Latency.h`
    # anchors in that document (`:25-31` and `:96-99`) are hand-maintained; a
    # clean run of this gate says nothing about either. Recorded here because the
    # gate exiting 0 across both bases is equally consistent with "the anchor is
    # fine" and with "the anchor was never examined", and only this comment
    # distinguishes them.
    ("docs/architecture/design-decisions/ADR-0013-release-trim-reaches-auto-poles.md",
     "src/dsp/AnabasisEngine.cpp:518-520"),
    ("docs/architecture/design-decisions/ADR-0014-frozen-trim-restore.md",
     "src/dsp/AnabasisEngine.cpp:262-278"),
    ("docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md",
     "src/PluginProcessor.cpp:25-26"),
    ("docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md",
     "src/PluginProcessor.h:111"),
    # `SERIALIZATION_REGISTRY.md` was re-aimed in the same round but was INVISIBLE
    # to a local run against `origin/main`: that base has a different citation
    # COUNT for this document, so it took the ordinal-pairing fallback, which only
    # judges base spellings still present verbatim — and a re-aim changes the
    # spelling. CI compares against the PREVIOUS PUSH, where the count matches and
    # ordinal pairing engages, so the gate flagged the lot. Checking one base is
    # not checking the gate; `TESTING.md` now says to run both.
    #
    # EIGHT ENTRIES WERE REMOVED FROM HERE ON 2026-08-14, and why they existed is
    # worth one line: they named INTERMEDIATE spellings — `:1544-1600`, `:916`,
    # `:1183-1196`, `:1057-1124`, `:1050-1053`, `:1582-1596`, `:1602-1793`,
    # `:1672` — that later commits in the same branch re-aimed again. A
    # declaration names a spelling; once the document stops carrying it, the entry
    # can never match and only prints a note on every run. Every one was confirmed
    # absent from `docs/` and the root Markdown before deletion.
    #
    # They were also, together with the count-mismatch fallback, the reason THIS
    # document's re-anchoring got no machine verification this round. So its
    # sixteen anchors were read by hand on 2026-08-14 and every one lands on the
    # symbol its prose names — `copyStateWithRaw` at `:989`, `saveSlotFromLive` at
    # `:1130`, `reassertFromRaw` at `:1256`, `getStateInformation` at `:1631`,
    # `setStateInformation` at `:1689`, `liveSurfaceRestored` at `:1742`, the
    # `SLOT` walk at `:1759`, `kUndoCap` at `PluginProcessor.h:453`, and the rest.
    # That is the check the tool could not make, made.
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:8"),
    # 2026-08-14: `applySlotToLive` gained the payload guard that makes ADR-0026's
    # read rule local instead of caller-dependent, moving everything below it by
    # 16 lines. Re-anchored in the SAME change set and declared here in the same
    # commit — the two halves of the rule, because a repair and a slip are the
    # same event to a text-identity test, and the round that fixed one anchor
    # without declaring it turned the fix itself into the next red gate. Each
    # target read by hand after the `--fix`: `liveSurfaceRestored` at `:1758`,
    # the stored-slot `ANABASIS` gate at `:1802`, the active-slot adopt at
    # `:1821`, and both `presetBaseline = presetShapeFromLive()` seedings that
    # `closePresetUndoBracket`'s own comment cites, at `:1537` and `:1607`.
    ("docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md",
     "src/PluginProcessor.cpp:1758"),
    ("docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md",
     "src/PluginProcessor.cpp:1802"),
    ("docs/architecture/design-decisions/ADR-0026-slot-payload-read-rules.md",
     "src/PluginProcessor.cpp:1821"),
    ("src/PluginProcessor.cpp",
     "src/PluginProcessor.cpp:1537"),
    ("src/PluginProcessor.cpp",
     "src/PluginProcessor.cpp:1607"),
])


def is_declared_reaim(doc, whole_base, whole_cur):
    """Does a declaration excuse this citation's mismatch?

    TWO conditions, and the second is what stops the list becoming a set of
    permanent exemptions:

      1. Either spelling is declared. Which side of the change a declaration
         names should not decide whether it works — the two check paths had
         drifted apart on exactly that, one testing the current spelling and the
         other the base one, so a declaration written for one branch was inert in
         the other and nothing said so.
      2. THE SPELLING ACTUALLY CHANGED. A re-aim moves an anchor, so it always
         changes the spelling. If base and current read the same, this diff did
         not re-aim anything and a mismatch is ordinary drift — the exact case a
         spelling-keyed declaration would otherwise silence forever. Without this
         the entry survived its own transition: once the base carried the
         re-aimed spelling, `1766 == 1766` kept matching and the next commit that
         moved that code had its drift swallowed by a declaration written for
         something else.

    So a declaration is good for exactly one transition, and the run after it
    reports the entry as removable.
    """
    if whole_base == whole_cur:
        return False
    return any((doc, w) in DELIBERATE_REAIMS for w in (whole_base, whole_cur) if w)


def classify(prefix, path):
    """The tracked path this citation names, or None if it is not ours.

    A citation qualified by anything — another checkout, or a pinned revision
    such as `7686204:` — is NOT about this working tree's current line numbers,
    and rewriting it is destroying evidence rather than maintaining it. That is
    not a hypothetical: it happened, to 27 anchors across `DESIGN.md`, five ADRs
    and `OPEN_QUESTIONS.md`, including the ADR-0016 table whose heading says in
    as many words that it was read from the pre-change tree.

    Requiring the path to be `TRACKED` verbatim is what makes an explicit
    foreign-root test unnecessary: a path from another checkout does not match
    this repository's own layout, so it is declined for the same reason a typo
    would be — this tool only ever rewrites the nine files it knows.
    """
    if prefix:
        return None
    if path.startswith(("/", "\\")):
        return None
    norm = path.replace("\\", "/")
    return norm if norm in TRACKED else None


def git_show(ref, path):
    r = subprocess.run(["git", "show", f"{ref}:{path}"],
                       capture_output=True, text=True)
    return r.stdout.split("\n") if r.returncode == 0 else None


def read(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def line_of(lines, n):
    return lines[n - 1] if lines is not None and 1 <= n <= len(lines) else None


# A cross-product ATTRIBUTION line, which `ADR-0009` mandates in exactly this
# spelling at the head of every adapted file. It carries an anchor into the
# SIBLING product spelled with a path this repository also owns:
#
#     // Provenance (ADR-0009): adapted from <the sibling product> src/gui/LookAndFeel.cpp:1-912 @ <sha>.
#
# The ownership test cannot see the difference. `<prefix>` catches a `rev:`
# qualifier and the lookbehind catches a path glued to another token, but here
# the product name is a separate word followed by a space, so the citation
# classifies as OURS and a re-anchor would rewrite the sibling's line range using
# THIS tree's code movement — the exact corruption this file's header says is the
# one it must never commit. Harmless while only `docs/` was scanned, and live the
# moment source files joined the scan, which is why the two changes are one
# change.
PROVENANCE_MARKER = "Provenance (ADR-0009)"


def citations(text):
    """(whole matched string, tracked path, span, [(start, end|None), ...])"""
    out = []
    # Byte offsets of every provenance BLOCK, so a match can be tested against
    # the block it sits in without re-splitting the text per match.
    #
    # THE BLOCK, NOT THE LINE, and the difference is a live corruption hazard
    # rather than tidiness. This exclusion was line-scoped, which happens to
    # cover `LookAndFeel.h:1` and `LookAndFeel.cpp:1` — marker and anchor share
    # a line there. It does NOT cover `src/gui/PluginEditor.h`, where the marker
    # sits on one line and the sibling's anchor (`src/PluginEditor.h:36-175 /
    # .cpp:672-800`) wraps onto the line two below. That file is safe today only
    # by ACCIDENT of ownership: `src/PluginEditor.h` is the sibling's spelling
    # and is not in `TRACKED`, this tree's editor being `src/gui/PluginEditor.h`.
    # The day a provenance sentence wraps a path this repo also owns, a re-anchor
    # would rewrite the SIBLING's range using this tree's movement — the one
    # corruption this file's header says it must never commit, reached by a
    # line break.
    #
    # A block is the marker's line plus every following CONTINUATION line: one
    # whose body, after a leading `//`, `#`, `*` or `>` and whitespace, is not
    # empty. That ends the block at the `//` separator below the sentence in
    # `PluginEditor.h`, at a blank line in Markdown, and at the first line of
    # real content anywhere else — without this function needing to know which
    # comment syntax it is reading.
    provenance_spans = []
    lines = text.splitlines(keepends=True)
    offsets, pos = [], 0
    for line in lines:
        offsets.append(pos)
        pos += len(line)

    # A continuation must wear the SAME comment prefix as the marker line, and
    # that requirement is the fix for a boundary that was merely heuristic. The
    # first form stripped `//`, `#`, `*` and `>` interchangeably, so in
    # `src/gui/LookAndFeel.h` — marker on line 1, `#pragma once` on line 2 —
    # `#pragma once` stripped to `pragma once`, read as a continuation, and the
    # block silently swallowed a line of real code. Under-checking rather than
    # corruption, since the effect is to DECLINE citations, but a block that can
    # extend over arbitrary following content is not a boundary.
    def _prefix_of(line):
        s = line.lstrip()
        for p in ("//", "#", "*", ">"):
            if s.startswith(p):
                return p
        return ""                      # Markdown prose: no comment marker

    def _is_continuation(line, prefix):
        s = line.lstrip()
        if prefix == "":
            return s != ""             # a wrapped sentence; a blank line ends it
        if not s.startswith(prefix):
            return False
        return s[len(prefix):].strip() != ""

    i = 0
    while i < len(lines):
        if PROVENANCE_MARKER in lines[i]:
            prefix = _prefix_of(lines[i])
            j = i + 1
            while j < len(lines) and _is_continuation(lines[j], prefix):
                j += 1
            provenance_spans.append((offsets[i], offsets[j - 1] + len(lines[j - 1])))
            i = j
            continue
        i += 1

    for m in CITATION.finditer(text):
        tracked = classify(m.group("prefix"), m.group("path"))
        if tracked is None:
            continue
        s, _e = m.span()
        if any(a <= s < b for a, b in provenance_spans):
            continue        # the sibling product's anchor — never ours to move
        anchors = [(int(a.group(1)), int(a.group(2)) if a.group(2) else None)
                   for a in ANCHOR.finditer(m.group("anchors"))]
        out.append((m.group(0), tracked, m.span(), anchors))
    return out


def line_map_from_diff(diff):
    """old line -> new line, from `git diff -U0` text. None inside an edited hunk.

    SPLIT OUT OF `build_line_map` SO A TEST CAN REACH IT (0.2.0). The arithmetic
    here is the whole correctness of the tool -- every re-anchor this file writes
    is this function's answer -- and until it was separated from the subprocess
    that feeds it, the only way to exercise it was to construct a git history.
    That is the same rule the GUI headers state about expressions reachable only
    from `paint`: a version no test can pin is a version that drifts.
    """
    edits = [(int(h.group(1)), int(h.group(2) or 1), int(h.group(4) or 1))
             for h in re.finditer(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@",
                                  diff, re.M)]

    def m(n):
        off = 0
        for start, old_count, new_count in edits:
            if old_count == 0:
                # A PURE INSERTION (`@@ -9,0 +10,2 @@`) sits AFTER old line
                # `start`; that line does not move. `start + old_count - 1` is
                # `start - 1` here, so the general test below would treat line
                # `start` as shifted — one line too far.
                if start < n:
                    off += new_count
                continue
            if start + old_count - 1 < n:
                off += new_count - old_count
            elif start <= n:
                return None
            else:
                break
        return n + off
    return m


def build_line_map(base, path):
    """old line -> new line for `path` between `base` and the working tree."""
    diff = subprocess.run(["git", "diff", "-U0", base, "--", path],
                          capture_output=True, text=True).stdout
    return line_map_from_diff(diff)


# `worklogs/` is deliberately OUT of scope. Those files are research records
# about the SIBLING product, and they cite it both fully qualified (an absolute
# path into that checkout, `…/src/PluginEditor.cpp:797-800`) and, in the same
# bullet, by bare file name as shorthand for the same file. The ownership test
# above now declines both shapes anyway; the exclusion stays as the cheap,
# explicit statement of intent, so a future loosening of that test cannot reach
# these files by accident.
EXCLUDED_PREFIXES = ("worklogs/",)


def doc_files():
    """Every file whose citations are checked — Markdown AND the tracked SOURCE.

    THE SOURCE HALF WAS ADDED AFTER A COMMENT DRIFTED 24 LINES INSIDE THE ROUND
    THAT BUILT THIS TOOL. `closePresetUndoBracket`'s argument named `:1482` and
    `:1538` as the two sites that seed `presetBaseline`; a comment block inserted
    above them moved both by 24, and because this scan covered only `docs/` and
    the root Markdown, the gate could not see it. A tool that keeps documents
    honest about the code while the code lies about itself is checking the
    smaller half: these anchors are read by whoever is editing the function, at
    the moment they are deciding whether the argument still holds.

    The files in `TRACKED` are exactly the ones already treated as citable
    targets, so this makes the set self-checking rather than widening it. A
    source file citing ITSELF is fine: a re-anchor rewrites digits inside one
    line and never changes a line COUNT, so the numbering the citation is
    measured against survives its own repair.
    """
    # `splitlines`, not `split()`: a tracked path may legally contain a space,
    # and whitespace-splitting would shatter it into fragments that match
    # nothing — silently dropping the file from the scan rather than failing.
    # No such path exists here today, which is exactly why the bug would ship.
    tracked = subprocess.run(["git", "ls-files"],
                             capture_output=True, text=True).stdout.splitlines()
    out = []
    for p in tracked:
        if p.startswith(EXCLUDED_PREFIXES):
            continue
        if p in TRACKED:
            out.append(p)          # the source half — see the docstring
            continue
        if not p.endswith(".md"):
            continue
        if p.startswith("docs/") or "/" not in p:
            out.append(p)
    return out


def apply_edits(text, edits):
    """Substitute by SPAN, right to left.

    Never by string. `str.replace` matches a prefix: with `src/PluginProcessor.cpp:107`
    drifted to `:130` and `src/PluginProcessor.cpp:1076` still correct, replacing
    the first by text turns the second into `src/PluginProcessor.cpp:1306` —
    corrupting a citation that had nothing wrong with it. Spans come from the
    match that produced them, so each rewrite lands on exactly its own citation.

    DEDUPED AND CHECKED FOR OVERLAP, because a span is only safe once. The
    count-mismatch branch pairs a base citation against ALL current spans
    carrying its spelling, so a document that spells one citation twice queued
    that span-list once per base occurrence — and applying an identical
    `(start, end, new)` twice does not repeat a rewrite, it destroys one: the
    first pass changes the text's length, so `end` no longer bounds what it
    bounded and the second splices the replacement into the middle of itself
    (`…cpp:2000` became `…cpp:20000`). A set fixes the duplicate; the overlap
    check is there because any FUTURE way of generating two different rewrites
    for one region fails the same way, and this function must not corrupt a
    governed document in silence when that happens.
    """
    uniq = sorted(set(edits), key=lambda e: -e[0])
    for (a_start, a_end, _), (b_start, b_end, _) in zip(uniq, uniq[1:]):
        if b_end > a_start:
            raise ValueError(
                f"check-citations: refusing to rewrite overlapping spans "
                f"[{b_start},{b_end}) and [{a_start},{a_end}) — this would corrupt the "
                f"document rather than re-anchor it")
    for start, end, new in uniq:
        text = text[:start] + new + text[end:]
    return text


def self_test():
    """Prove the checker still recognises, still maps and still rewrites.

    TESTING_POLICY rule 4, applied to the gate whose SILENCE is read on every
    push. A citation checker that has stopped matching anything reports "N
    anchor(s) still point at the same text" with N = 0 and exits 0 -- which is
    indistinguishable from a clean tree, and is the shape this repository's
    lints are written against. Nothing here touches git or the working tree: the
    three halves that can silently stop working are the CITATION pattern, the
    `line_map_from_diff` arithmetic and `apply_edits`, and all three are pure.

    Both directions are pinned throughout. A recogniser that only proves it can
    match drifts into rewriting other people's line numbers -- which this tool
    has done once already, to 27 anchors -- so every ownership case below also
    asserts what must NOT be claimed.
    """
    failures = checked = 0

    def check(label, got, want):
        nonlocal failures, checked
        checked += 1
        if got != want:
            failures += 1
            print(f"self-test FAIL: {label}: got {got!r}, want {want!r}", file=sys.stderr)

    # --- 1. RECOGNITION: what the pattern is and is not allowed to match ------
    def found(text):
        return [(m.group("prefix"), m.group("path"), m.group("anchors"))
                for m in CITATION.finditer(text)]

    check("a plain anchor is recognised",
          found("see src/PluginProcessor.cpp:695 for it"),
          [(None, "src/PluginProcessor.cpp", "695")])
    check("a range is recognised",
          found("src/PluginProcessor.cpp:695-752"),
          [(None, "src/PluginProcessor.cpp", "695-752")])
    # THE COMPOUND FORM is what an early version of this file missed: it
    # re-anchored the first anchor and left the bare numbers behind it, producing
    # an out-of-order, internally contradictory citation.
    check("a compound list is one match carrying every anchor",
          found("src/PluginProcessor.cpp:708-709, 851, 1208"),
          [(None, "src/PluginProcessor.cpp", "708-709, 851, 1208")])
    # THE QUALIFIER MUST BE CAPTURED, not skipped past. Without the lookbehind
    # the scan matched from `src/...` and the qualifier never reached
    # `classify()`, so another product's anchors were rewritten with this tree's
    # code movement.
    check("a revision qualifier is captured as the prefix",
          found("7686204:src/PluginProcessor.cpp:485-491"),
          [("7686204", "src/PluginProcessor.cpp", "485-491")])
    check("a checkout qualifier is captured as the prefix",
          found("some-checkout:src/PluginProcessor.cpp:485"),
          [("some-checkout", "src/PluginProcessor.cpp", "485")])
    # ...and rejecting the qualified match is not enough on its own: WITHOUT the
    # lookbehind the scan retries one character later and matches a TRUNCATED
    # path -- `rc/PluginProcessor.cpp` -- which classifies as not-ours by luck
    # rather than by rule. The lookbehind blocks every one of those restarts,
    # because each is preceded by a path or word character, so an absolute path
    # produces no match at all rather than a mangled one.
    check("an absolute path yields no match and no restart",
          found("/abs/path/src/PluginProcessor.cpp:485"), [])
    # RECOGNITION AND OWNERSHIP ARE SEPARATE STAGES, and this is the case that
    # shows why the second one has to exist. At offset 0 there is nothing for the
    # lookbehind to reject, so `xsrc/PluginProcessor.cpp` IS matched -- and it is
    # `classify()`, testing the path against TRACKED verbatim, that declines it.
    check("a near-miss path is matched but not owned",
          [(pth, classify(pre, pth)) for pre, pth, _ in found("xsrc/PluginProcessor.cpp:485")],
          [("xsrc/PluginProcessor.cpp", None)])
    check("a bare file name is not a citation",
          found("PluginProcessor.cpp:7"), [])
    check("prose with a colon and a number is not a citation",
          found("see section 4: 7 of them"), [])

    # --- 2. OWNERSHIP: classify() decides what may be rewritten ---------------
    check("a tracked path is ours", classify(None, "src/PluginProcessor.cpp"),
          "src/PluginProcessor.cpp")
    check("a windows spelling of a tracked path normalises",
          classify(None, "src\\gui\\PluginEditor.cpp"), "src/gui/PluginEditor.cpp")
    check("an untracked path is not ours", classify(None, "src/dsp/CeilingClamp.h"), None)
    # THE SIBLING PRODUCT'S EDITOR lives at `src/PluginEditor.cpp`; ours is
    # `src/gui/PluginEditor.cpp`. The spellings differ, which is what makes the
    # verbatim-TRACKED test sufficient without a foreign-root rule.
    check("the sibling product's editor path is not ours",
          classify(None, "src/PluginEditor.cpp"), None)
    check("a qualified citation is never ours",
          classify("7686204", "src/PluginProcessor.cpp"), None)
    check("an absolute path is never ours",
          classify(None, "/home/x/src/PluginProcessor.cpp"), None)

    # --- 3. THE PROVENANCE BLOCK: the sibling's anchors are never ours --------
    # BLOCK-SCOPED, NOT LINE-SCOPED. The marker and the anchor share a line in
    # two files and do NOT in a third, where the sibling's range wraps two lines
    # below the marker. A line-scoped exclusion is safe there only by accident of
    # which spellings this repository happens to own.
    prov_same_line = ("// Provenance (ADR-0009): adapted from the sibling "
                      "src/gui/LookAndFeel.cpp:1-912 @ abc123.\n")
    check("an anchor on the provenance line is excluded",
          citations(prov_same_line), [])
    prov_wrapped = ("// Provenance (ADR-0009): adapted from the sibling product\n"
                    "// at revision abc123, covering\n"
                    "// src/gui/PluginEditor.cpp:36-175 and the rest.\n")
    check("an anchor two lines below the marker is excluded too",
          citations(prov_wrapped), [])
    # ...and the exclusion must END. A citation after the block is ours again.
    after = prov_same_line + "\nOur own note: src/PluginProcessor.cpp:695.\n"
    check("a citation after the provenance block is still checked",
          [c[1] for c in citations(after)], ["src/PluginProcessor.cpp"])

    # --- 4. MAPPING: line_map_from_diff, every hunk shape --------------------
    def hunk(*specs):
        return "".join(f"@@ -{a} +{b} @@\n" for a, b in specs)

    m = line_map_from_diff("")
    check("no diff moves nothing", [m(1), m(500)], [1, 500])

    # A PURE INSERTION sits AFTER the old line it names, so that line does not
    # move. `start + old_count - 1` is `start - 1` here, and the general test
    # would shift line `start` one line too far.
    m = line_map_from_diff(hunk(("9,0", "10,2")))
    check("a pure insertion does not move the line it follows", m(9), 9)
    check("a pure insertion shifts the lines after it", m(10), 12)

    m = line_map_from_diff(hunk(("10,3", "10,0")))
    check("a deletion pulls later lines up", m(20), 17)
    check("a line inside a deleted hunk has no image", m(11), None)
    check("a line before a deletion is unmoved", m(9), 9)

    m = line_map_from_diff(hunk(("10,2", "10,5")))
    check("an edited hunk's own lines have no image", m(10), None)
    check("lines after an edited hunk shift by the size delta", m(20), 23)

    # Several hunks accumulate, and a `,1` count is implicit in the short form.
    m = line_map_from_diff(hunk(("5,0", "6,3"), ("40", "43")))
    check("offsets accumulate across hunks", m(100), 103)
    check("a single-line hunk parses as count 1", m(40), None)

    # --- 5. REWRITING: apply_edits ------------------------------------------
    # BY SPAN, RIGHT TO LEFT, NEVER BY STRING. `str.replace` matches a prefix:
    # rewriting `...cpp:107` by text turns a correct `...cpp:1076` into
    # `...cpp:1306`.
    text = "a:107 and a:1076"
    check("spans rewrite only their own citation",
          apply_edits(text, [(0, 5, "a:130")]), "a:130 and a:1076")
    check("two spans both land",
          apply_edits(text, [(0, 5, "a:130"), (10, 16, "a:2000")]),
          "a:130 and a:2000")
    # A DUPLICATE SPAN MUST NOT BE APPLIED TWICE: the first pass changes the
    # text's length, so the second splices the replacement into itself.
    check("an identical edit queued twice is applied once",
          apply_edits("a:2000", [(0, 6, "a:2001"), (0, 6, "a:2001")]), "a:2001")
    overlapped = False
    try:
        apply_edits("a:2000", [(0, 6, "a:2001"), (2, 6, "a:9")])
    except ValueError:
        overlapped = True
    check("overlapping rewrites raise instead of corrupting", overlapped, True)

    # --- 6. DECLARED RE-AIMS: good for exactly one transition ----------------
    doc, base, cur = "docs/X.md", "src/PluginProcessor.cpp:100", "src/PluginProcessor.cpp:200"
    saved = set(DELIBERATE_REAIMS)
    try:
        DELIBERATE_REAIMS.clear()
        check("an undeclared move is not excused", is_declared_reaim(doc, base, cur), False)
        DELIBERATE_REAIMS.add((doc, cur))
        check("a declaration naming the current spelling excuses it",
              is_declared_reaim(doc, base, cur), True)
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.add((doc, base))
        check("a declaration naming the base spelling excuses it too",
              is_declared_reaim(doc, base, cur), True)
        # THE SPELLING MUST ACTUALLY HAVE CHANGED. Once the base carries the
        # re-aimed anchor, `200 == 200` kept matching and swallowed the drift of
        # whatever moved that code NEXT -- so the entry survived its own
        # transition and silenced an unrelated failure.
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.add((doc, cur))
        check("a declaration is inert once base and current agree",
              is_declared_reaim(doc, cur, cur), False)
        # ...and it is scoped to its document.
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.add(("docs/OTHER.md", cur))
        check("a declaration does not excuse another document",
              is_declared_reaim(doc, base, cur), False)
    finally:
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.update(saved)

    if failures:
        print(f"\ncheck-citations: {failures} of {checked} self-test case(s) failed.",
              file=sys.stderr)
        return 1
    print(f"check-citations: self-test passed ({checked} cases).")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Verify documentation evidence anchors.")
    ap.add_argument("--base", default="origin/main",
                    help="revision the citations were last verified against")
    # PROVE THE CHECKER IS LIVE BEFORE TRUSTING ITS SILENCE (TESTING_POLICY
    # rule 4). This gate's clean output is a COUNT, and a recogniser that has
    # stopped matching prints zero and exits 0 -- the same text a genuinely
    # clean tree prints. It runs in `source-lint`, immediately before the check
    # it vouches for, because a liveness proof in another job proves nothing
    # about this run.
    ap.add_argument("--self-test", action="store_true",
                    help="run the checker's own test cases and exit")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true",
                      help="report drifted citations (the default)")
    mode.add_argument("--fix", action="store_true",
                      help="re-anchor drifted citations instead of only reporting")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    base_src, now_src = {}, {}
    for path in TRACKED:
        if not os.path.exists(path):
            continue
        b = git_show(args.base, path)
        if b is None:
            print(f"check-citations: {path} does not exist at {args.base}; skipping",
                  file=sys.stderr)
            continue
        base_src[path] = b
        now_src[path] = read(path).split("\n")

    if not base_src:
        print(f"check-citations: nothing to check against {args.base}", file=sys.stderr)
        return 2

    maps = {p: build_line_map(args.base, p) for p in base_src}

    total = drifted = fixable = unmappable = unchecked = 0
    used_reaims = set()
    base_doc_cites = {}                # doc -> the citation spellings the BASE carries
    for doc in doc_files():
        base_text = subprocess.run(["git", "show", f"{args.base}:{doc}"],
                                   capture_output=True, text=True)
        if base_text.returncode != 0:
            continue                       # new document: nothing to drift from
        old_cites = citations(base_text.stdout)
        base_doc_cites[doc] = {c[0] for c in old_cites}
        text = read(doc)
        cur_cites = citations(text)

        # Paired PER PATH, in order of appearance. Neither global position nor
        # the citation string works on its own: a change set may add or remove
        # citations (position breaks), and once a citation has been re-anchored
        # its base spelling is gone from the file, so looking for that string
        # makes every LATER shift invisible — which is precisely how this round's
        # anchors went stale twice. Pairing the Nth `PluginProcessor.cpp`
        # reference in the base with the Nth in the current file survives both.
        by_path_old, by_path_cur = {}, {}
        for c in old_cites:
            by_path_old.setdefault(c[1], []).append(c)
        for c in cur_cites:
            by_path_cur.setdefault(c[1], []).append(c)

        edits = []
        for tracked, olds in by_path_old.items():
            if tracked not in base_src:
                continue
            curs = by_path_cur.get(tracked, [])
            if len(curs) != len(olds):
                # A change set is allowed to ADD or REMOVE citations, so a count
                # change is not itself a failure. Ordinal pairing is no longer
                # meaningful, though, so fall back to the conservative check:
                # every base citation whose spelling is still present must still
                # be correct. A citation that was re-spelled or removed is beyond
                # what this can judge, and a NEW one has nothing to drift from.
                still = {}
                for (whole_c, _t, span_c, _a) in curs:
                    still.setdefault(whole_c, []).append(span_c)
                # A spelling the BASE carries more than once. `still` is keyed by
                # spelling, so every base occurrence of the same citation resolves
                # to the same span list and the same rewrite — counting each of
                # them would report more citations drifted and fixed than the
                # document contains. `apply_edits` de-duplicates the spans, so
                # this was only ever a reporting defect; it is still the class of
                # imprecision this tool exists to remove from documents.
                counted = set()
                for (whole_o, _t, _span_o, anchors_o) in olds:
                    # `total` counts every base anchor on BOTH paths, and the ones
                    # this path cannot judge are counted again into `unchecked`
                    # and reported. Counting only the judged ones here made the
                    # closing "N of M" mean a different thing per document
                    # depending on which branch it took — the summary of a tool
                    # whose subject is documents saying what they mean.
                    total += len(anchors_o)
                    spans = still.get(whole_o)
                    if not spans:
                        unchecked += len(anchors_o)
                        continue
                    if all(line_of(base_src[tracked], a) == line_of(now_src[tracked], a)
                           and (b is None or line_of(base_src[tracked], b) == line_of(now_src[tracked], b))
                           for (a, b) in anchors_o):
                        continue
                    # Past this point the citation is DRIFTED and will be counted
                    # and queued. A second base occurrence of the same spelling
                    # reaches here with the same spans and the same rebuild, so it
                    # contributes nothing but inflated numbers — its anchors are
                    # already in `total` above, which is the figure that must stay
                    # per-occurrence.
                    if whole_o in counted:
                        continue
                    counted.add(whole_o)
                    # No DELIBERATE_REAIMS check here on purpose: this branch
                    # only reaches citations whose spelling is UNCHANGED (that is
                    # the `still.get(whole_o)` filter above), and a re-aim always
                    # changes the spelling. Anything mismatching here is drift.
                    mapped, movable = [], True
                    for (a, b) in anchors_o:
                        na, nb = maps[tracked](a), (maps[tracked](b) if b else None)
                        if na is None or (b is not None and nb is None):
                            movable = False
                            break
                        mapped.append((na, nb))
                    drifted += 1
                    if not movable:
                        unmappable += 1
                        print(f"  UNMAPPABLE {doc}: {whole_o} "
                              f"(the cited lines were themselves edited — re-aim by hand)")
                        continue
                    rebuilt = f"{tracked}:" + ", ".join(
                        f"{na}-{nb}" if nb else f"{na}" for na, nb in mapped)
                    # One citation, however many times the document spells it.
                    # Identical spellings carry identical anchors, so the same
                    # rewrite is right for each occurrence — but they are ONE
                    # drifted citation, and counting the spans inflated `fixed`
                    # past the number of citations there were to fix.
                    if len(spans) > 1:
                        print(f"    (spelled {len(spans)}× in this document; all move together)")
                    print(f"  DRIFTED {doc}: {whole_o} -> {rebuilt}")
                    edits += [(s, e, rebuilt) for (s, e) in spans]
                    fixable += 1
                print(f"check-citations: note — {doc} now has {len(curs)} `{tracked}` "
                      f"citation(s) where {args.base} had {len(olds)}; the added ones are "
                      f"not checkable against that base.")
                continue

            for (whole_o, _to, _so, anchors_o), (whole_c, _tc, span_c, anchors_c) in zip(olds, curs):
                total += len(anchors_o)
                if len(anchors_o) != len(anchors_c):
                    # COUNTS AS UNMAPPABLE, not merely drifted, and the difference
                    # is the exit code. `--check` returns 1 on any drift; `--fix`
                    # returns `2 if unmappable else 0`. This branch declines to
                    # touch the citation — a `:10` that became `:10-20` is a
                    # judgement about what the prose means, not a line shift — so
                    # counting it as drift alone made `--fix` exit 0 while
                    # promising a clean repair, and the very next `--check` (CI's)
                    # went red on what it left behind. The documented workflow is
                    # "run `--fix` in the SAME change set", so `--fix` saying 0 is
                    # the last word a contributor hears before pushing.
                    print(f"  ANCHOR COUNT {doc}: {whole_o} -> {whole_c}; review by hand")
                    drifted += 1
                    unmappable += 1
                    continue

                # Correct means: the text at the CURRENT anchors is the text the
                # BASE anchors named.
                same = all(line_of(base_src[tracked], a) == line_of(now_src[tracked], a2)
                           and (b is None) == (b2 is None)
                           and (b is None or line_of(base_src[tracked], b) == line_of(now_src[tracked], b2))
                           for (a, b), (a2, b2) in zip(anchors_o, anchors_c))
                if same:
                    continue

                if is_declared_reaim(doc, whole_o, whole_c):
                    # BOTH spellings, because the declaration may have been
                    # written with either (that is what `is_declared_reaim`
                    # accepts). Recording only the current one left an entry
                    # declared with the BASE spelling honoured here and then
                    # ALSO listed in `DELIBERATE_REAIMS - used_reaims`, which
                    # prints "was not needed … delete it" — advice that, taken,
                    # re-breaks the gate. Latent today (every entry happens to be
                    # a current spelling) and cheaper to close than to remember.
                    used_reaims.add((doc, whole_c))
                    used_reaims.add((doc, whole_o))
                    continue

                mapped, movable = [], True
                for (a, b) in anchors_o:
                    na, nb = maps[tracked](a), (maps[tracked](b) if b else None)
                    if na is None or (b is not None and nb is None):
                        movable = False
                        break
                    mapped.append((na, nb))

                drifted += 1
                if not movable:
                    unmappable += 1
                    print(f"  UNMAPPABLE {doc}: {whole_c} "
                          f"(the cited lines were themselves edited — re-aim by hand)")
                    continue
                rebuilt = f"{tracked}:" + ", ".join(
                    f"{na}-{nb}" if nb else f"{na}" for na, nb in mapped)
                print(f"  DRIFTED {doc}: {whole_c} -> {rebuilt}")
                edits.append((span_c[0], span_c[1], rebuilt))
                fixable += 1

        if args.fix and edits:
            with open(doc, "w", encoding="utf-8") as fh:
                fh.write(apply_edits(text, edits))
            # `now_src` IS THE WORKING TREE, and it stopped being so the moment a
            # TRACKED SOURCE joined the scanned set. The snapshot is taken once,
            # before this loop; a `--fix` that rewrites `src/PluginProcessor.cpp`
            # leaves the cached copy holding pre-rewrite lines, and any document
            # processed LATER that cites one of the rewritten lines is then judged
            # against text no longer on disk. Narrow — it needs a citation whose
            # own line is a citation — but it is an invariant, and an invariant
            # this tool holds about a file it just edited is exactly the kind it
            # cannot afford to be casually wrong about.
            if doc in now_src:
                now_src[doc] = read(doc).split("\n")

                # …AND THE REFRESH IS ONLY HALF OF IT. Keeping the cache in step
                # with disk fixes what this run judges NEXT; it does nothing about
                # what the rewrite DID. Re-anchoring a citation that lives inside a
                # tracked source line changes that line's TEXT, and every anchor
                # aimed at that line is verified by text identity — so a document
                # elsewhere pointing there now compares the base's old wording
                # against the new one and reports drift, or gets re-aimed away from
                # a line that never moved. The repair is indistinguishable from the
                # damage, which is this tool's recurring shape.
                #
                # Not silently, then. The lines are named, because a human can
                # settle in seconds what the tool cannot settle at all: whether
                # anything aims at them. Narrow by construction — it needs an
                # anchor whose target line is itself a citation — but that is
                # exactly the shape of the comment in `closePresetUndoBracket`
                # that motivated scanning source in the first place.
                touched = sorted({text.count("\n", 0, a) + 1 for a, _b, _r in edits})
                print(f"check-citations: NOTE — {doc} is a tracked SOURCE file and "
                      f"line(s) {', '.join(str(n) for n in touched)} were rewritten. "
                      f"Any citation elsewhere aimed at those lines is now measured "
                      f"against changed text; re-check them by hand.")

    # A declared re-aim is never silent: it is announced when it is honoured, and
    # announced again when it has stopped being needed, so the list cannot quietly
    # become a set of permanent exemptions.
    # INTERSECTED, because `used_reaims` deliberately holds BOTH spellings of an
    # honoured re-aim (see the `is_declared_reaim` branch) and only one of them is
    # a declaration. Reporting the set raw announced the UNDECLARED spelling too,
    # so a run printed twice as many accepted re-aims as `DELIBERATE_REAIMS` has
    # entries — a tool whose subject is documents saying exactly what they mean,
    # not saying exactly what it means. The intersection is never empty for an
    # honoured re-aim: `is_declared_reaim` returns true only when at least one of
    # the two spellings is literally in the set, so the "never silent" property
    # below survives the narrowing.
    for (doc, whole) in sorted(used_reaims & DELIBERATE_REAIMS):
        print(f"check-citations: ACCEPTED re-aim {doc}: {whole} "
              f"(declared in DELIBERATE_REAIMS — verify the aim by hand, not by this tool)")
    # "DELETE IT" IS ADVICE, so it is only given when it is SAFE, and the two
    # cases below were one message until 2026-08-14. An entry is unused against a
    # given base for two entirely different reasons:
    #
    #   * The base already CARRIES the re-aimed spelling — the transition this
    #     entry excused is behind that base, and `is_declared_reaim`'s "the
    #     spelling actually changed" test can never fire for it again from there.
    #   * The base is simply not the one that needed it, and does not carry the
    #     spelling either. It is a declaration for a DIFFERENT base.
    #
    # Neither is an instruction, and the old single message ("delete it once the
    # base carries the re-aim") read as one on both. CI compares against the
    # previous PUSH (`github.event.before`); on that base ~20 of 23 entries have
    # nothing to excuse, because they were written for the merge base. Acting on
    # that log re-breaks the gate against `origin/main` — the tool destroying what
    # it protects, in a green run. Even the first case is only safe when the base
    # in hand is the branch's MERGE BASE: an entry retired against the previous
    # push is very often still live against `main`, which is exactly how this
    # round's `PluginEditor.h:604` declaration was still doing real work.
    for (doc, whole) in sorted(DELIBERATE_REAIMS - used_reaims):
        if whole in base_doc_cites.get(doc, set()):
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
                  f"needed against {args.base}, which already carries the re-aimed "
                  f"spelling. Safe to delete ONLY if that base is the branch's merge "
                  f"base; re-run against that before removing it.")
        else:
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
                  f"exercised against {args.base}, which does not carry the re-aimed "
                  f"spelling either — the entry belongs to a different base. Keep it.")

    # Every number below counts base ANCHORS, and `unchecked` is stated rather
    # than netted off, because the difference between "17 verified" and "17
    # verified, 4 beyond what this could judge" is the whole value of the run.
    checked = total - unchecked
    tail = f" ({unchecked} re-spelled or removed, beyond what this run can judge)" if unchecked else ""

    if args.fix:
        print(f"\ncheck-citations: re-anchored {fixable} citation(s) across {checked} "
              f"checked anchor(s){tail}; {unmappable} need a human.")
        return 2 if unmappable else 0

    if drifted:
        print(f"\ncheck-citations: {drifted} citation(s) across {checked} checked "
              f"anchor(s){tail} no longer point at the text they did at {args.base}. "
              f"Re-anchor them in THIS change set (scripts/check-citations.py --fix), "
              f"then re-read the ones marked UNMAPPABLE.", file=sys.stderr)
        return 1

    print(f"check-citations: {checked} anchor(s) still point at the same text as "
          f"{args.base}{tail}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
