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
     "src/PluginProcessor.cpp:813-831"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:730"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:803-811"),
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:1832"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "tests/state_tests.cpp:2141"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.h:453"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.h:431-439"),
    ("docs/architecture/THREAD_MODEL.md",
     "src/gui/PluginEditor.h:591"),
    ("docs/architecture/design-decisions/ADR-0013-release-trim-reaches-auto-poles.md",
     "src/dsp/AnabasisEngine.cpp:518-520"),
    ("docs/architecture/design-decisions/ADR-0014-frozen-trim-restore.md",
     "src/dsp/AnabasisEngine.cpp:262-278"),
    ("docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md",
     "src/PluginProcessor.cpp:25-26"),
    ("docs/architecture/design-decisions/ADR-0015-pre-ship-contract-refreeze.md",
     "src/PluginProcessor.h:111"),
    # These nine were re-aimed in the same round but were INVISIBLE to a local
    # run against `origin/main`: that base has a different citation COUNT for
    # this document, so it took the ordinal-pairing fallback, which only judges
    # base spellings still present verbatim — and a re-aim changes the spelling.
    # CI compares against the PREVIOUS PUSH, where the count matches and ordinal
    # pairing engages, so the gate flagged all nine. Checking one base is not
    # checking the gate; `TESTING.md` now says to run both.
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1544-1600"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:8"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:916"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1183-1196"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1057-1124"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1050-1053"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1582-1596"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1602-1793"),
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1672"),
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


def citations(text):
    """(whole matched string, tracked path, span, [(start, end|None), ...])"""
    out = []
    for m in CITATION.finditer(text):
        tracked = classify(m.group("prefix"), m.group("path"))
        if tracked is None:
            continue
        anchors = [(int(a.group(1)), int(a.group(2)) if a.group(2) else None)
                   for a in ANCHOR.finditer(m.group("anchors"))]
        out.append((m.group(0), tracked, m.span(), anchors))
    return out


def build_line_map(base, path):
    """old line -> new line, from the diff hunks. None inside an edited hunk."""
    diff = subprocess.run(["git", "diff", "-U0", base, "--", path],
                          capture_output=True, text=True).stdout
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


# `worklogs/` is deliberately OUT of scope. Those files are research records
# about the SIBLING product, and they cite it both fully qualified (an absolute
# path into that checkout, `…/src/PluginEditor.cpp:797-800`) and, in the same
# bullet, by bare file name as shorthand for the same file. The ownership test
# above now declines both shapes anyway; the exclusion stays as the cheap,
# explicit statement of intent, so a future loosening of that test cannot reach
# these files by accident.
EXCLUDED_PREFIXES = ("worklogs/",)


def doc_files():
    tracked = subprocess.run(["git", "ls-files"],
                             capture_output=True, text=True).stdout.split()
    out = []
    for p in tracked:
        if not p.endswith(".md"):
            continue
        if p.startswith(EXCLUDED_PREFIXES):
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


def main():
    ap = argparse.ArgumentParser(description="Verify documentation evidence anchors.")
    ap.add_argument("--base", default="origin/main",
                    help="revision the citations were last verified against")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true",
                      help="report drifted citations (the default)")
    mode.add_argument("--fix", action="store_true",
                      help="re-anchor drifted citations instead of only reporting")
    args = ap.parse_args()

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
    for doc in doc_files():
        base_text = subprocess.run(["git", "show", f"{args.base}:{doc}"],
                                   capture_output=True, text=True)
        if base_text.returncode != 0:
            continue                       # new document: nothing to drift from
        old_cites = citations(base_text.stdout)
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
                    print(f"  ANCHOR COUNT {doc}: {whole_o} -> {whole_c}; review by hand")
                    drifted += 1
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

    # A declared re-aim is never silent: it is announced when it is honoured, and
    # announced again when it has stopped being needed, so the list cannot quietly
    # become a set of permanent exemptions.
    for (doc, whole) in sorted(used_reaims):
        print(f"check-citations: ACCEPTED re-aim {doc}: {whole} "
              f"(declared in DELIBERATE_REAIMS — verify the aim by hand, not by this tool)")
    for (doc, whole) in sorted(DELIBERATE_REAIMS - used_reaims):
        print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
              f"needed against {args.base}; delete it once the base carries the re-aim.")

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
