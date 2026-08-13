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
#      Preserving content identity faithfully preserves a pre-existing mistake,
#      and one such case is why this file's header says so out loud rather than
#      leaving the next reader to assume `--check` clean means every citation is
#      correct. It means none of them MOVED.
#
#  Exit codes follow the sibling scripts: 0 clean · 1 drift found (`--check`)
#  · 2 the check itself could not run.
# ============================================================================

import argparse
import os
import re
import subprocess
import sys

# The sources whose line anchors are worth tracking: the ones the architecture
# documents actually cite. A file absent here is simply not checked.
TRACKED = {
    "PluginProcessor.cpp": "src/PluginProcessor.cpp",
    "PluginProcessor.h":   "src/PluginProcessor.h",
    "PluginEditor.cpp":    "src/gui/PluginEditor.cpp",
    "PluginEditor.h":      "src/gui/PluginEditor.h",
    "LookAndFeel.cpp":     "src/gui/LookAndFeel.cpp",
    "LookAndFeel.h":       "src/gui/LookAndFeel.h",
    "state_tests.cpp":     "tests/state_tests.cpp",
    "dsp_tests.cpp":       "tests/dsp_tests.cpp",
    "AnabasisEngine.cpp":  "src/dsp/AnabasisEngine.cpp",
}

# A citation is `<path>:<line>` or `<path>:<start>-<end>`. The path is captured
# WITH whatever directory prefix precedes it, because a bare file name is
# ambiguous across repositories: the P0 research worklog cites the sibling
# product as `/home/user/Anamorph/src/PluginEditor.cpp:286-295`, and matching on
# the file name alone re-anchored those UPSTREAM anchors against THIS tree's line
# numbers — corrupting a historical record to fix a problem it does not have.
# `classify()` below is what decides a citation is ours.
CITATION = re.compile(r"(?P<path>[\w./\\-]*\b\w+\.\w+):(?P<start>\d+)(?:-(?P<end>\d+))?")

# A citation belongs to this repository only if its path is repo-relative (or
# bare) — never if it is rooted somewhere else.
FOREIGN_ROOT = re.compile(r"(^|[/\\])(Anamorph|home|Users|usr|opt|tmp)([/\\]|$)", re.I)


def owned_by_this_repo(path: str) -> bool:
    if path.startswith(("/", "\\")) or FOREIGN_ROOT.search(path):
        return False
    # `src/gui/PluginEditor.cpp`, `PluginEditor.cpp` and `../src/x.cpp` are ours;
    # anything naming another checkout is not.
    return True


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
    out = []
    for m in CITATION.finditer(text):
        path = m.group("path")
        name = os.path.basename(path.replace("\\", "/"))
        if name in TRACKED and owned_by_this_repo(path):
            out.append((path, name, int(m.group("start")),
                        int(m.group("end")) if m.group("end") else None))
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
            if start + old_count - 1 < n:
                off += new_count - old_count
            elif start <= n:
                return None
            else:
                break
        return n + off
    return m


# `worklogs/` is deliberately OUT of scope. Those files are research records
# about the SIBLING product, and they cite it both fully qualified
# (`/home/user/Anamorph/src/PluginEditor.cpp:797-800`) and, in the same bullet,
# by bare file name as shorthand for the same upstream file. A bare name there
# means "the file I have been quoting", not a file in this tree, so re-anchoring
# them against these line numbers would rewrite a historical record to match code
# it was never about. The governed documents — `docs/` and the root Markdown —
# are the ones whose citations point here.
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


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", default="origin/main",
                    help="revision the citations were last verified against")
    ap.add_argument("--fix", action="store_true",
                    help="re-anchor drifted citations instead of only reporting")
    args = ap.parse_args()

    base_src, now_src = {}, {}
    for name, path in TRACKED.items():
        if not os.path.exists(path):
            continue
        b = git_show(args.base, path)
        if b is None:
            print(f"check-citations: {path} does not exist at {args.base}; skipping",
                  file=sys.stderr)
            continue
        base_src[name] = b
        now_src[name] = read(path).split("\n")

    if not base_src:
        print(f"check-citations: nothing to check against {args.base}", file=sys.stderr)
        return 2

    maps = {n: build_line_map(args.base, TRACKED[n]) for n in base_src}

    total = drifted = fixed = unmappable = 0
    for doc in doc_files():
        base_text = subprocess.run(["git", "show", f"{args.base}:{doc}"],
                                   capture_output=True, text=True)
        if base_text.returncode != 0:
            continue                       # new document: nothing to drift from
        old_cites = citations(base_text.stdout)
        text = read(doc)

        # Matched by CITATION STRING, not by position. A change set is allowed to
        # ADD or REMOVE citations — this round adds a read-rule row — and a
        # positional walk would mis-pair every citation after the insertion and
        # report the whole file as drifted. For each citation the base had: if the
        # identical string is still present, its content is checked; if it is not,
        # the mapped string must be, and that is the re-anchored form. A citation
        # the base did not have is new and has nothing to drift from.
        replacements = []
        for (p1, f, a, b) in old_cites:
            if f not in base_src:
                continue
            total += 1
            cur = f"{p1}:{a}" + (f"-{b}" if b else "")
            na, nb = maps[f](a), (maps[f](b) if b else None)
            moved = (f"{p1}:{na}" + (f"-{nb}" if b else "")) if na is not None else None

            if moved is not None and moved != cur and moved in text:
                continue                      # already re-anchored in this change set

            if cur not in text:
                continue                      # citation removed or rewritten by hand

            same = (line_of(base_src[f], a) == line_of(now_src[f], a)
                    and (b is None or line_of(base_src[f], b) == line_of(now_src[f], b)))
            if same:
                continue

            drifted += 1
            if moved is None:
                unmappable += 1
                print(f"  UNMAPPABLE {doc}: {cur} "
                      f"(the cited lines were themselves edited — re-aim by hand)")
                continue
            print(f"  DRIFTED {doc}: {cur} -> {moved}")
            replacements.append((cur, moved))

        if args.fix and replacements:
            for cur, new in sorted(set(replacements), key=lambda x: -len(x[0])):
                text = text.replace(cur, new)
            with open(doc, "w", encoding="utf-8") as fh:
                fh.write(text)
            fixed += len(replacements)

    if args.fix:
        print(f"\ncheck-citations: re-anchored {fixed} of {total} citation(s); "
              f"{unmappable} need a human.")
        return 2 if unmappable else 0

    if drifted:
        print(f"\ncheck-citations: {drifted} of {total} citation(s) no longer point at "
              f"the text they did at {args.base}. Re-anchor them in THIS change set "
              f"(scripts/check-citations.py --fix), then re-read the ones marked "
              f"UNMAPPABLE.", file=sys.stderr)
        return 1

    print(f"check-citations: {total} citation(s) still point at the same text as "
          f"{args.base}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
