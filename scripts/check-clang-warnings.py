#!/usr/bin/env python3
# ============================================================================
#  check-clang-warnings.py — the first-party warning gate, with a self-test.
#
#  WHY THIS IS A SCRIPT AND NOT A `grep` IN THE WORKFLOW. The gate it replaces
#  was a single anchored expression:
#
#      grep -E "^${GITHUB_WORKSPACE}/(src|tests|tools)/[^:]*:[0-9]+:[0-9]+: warning:"
#
#  That expression WORKS in the configuration this repository builds — CMake's
#  Ninja generator hands Clang absolute source paths here, and CI has the
#  receipts: run 31449400277's `linux-clang` job failed on
#  `/home/runner/work/Anabasis/Anabasis/tools/channel_probe.cpp:202:5: warning:`.
#  The defect is not that it never matches. The defect is its SHAPE: it can only
#  match one spelling of a path the build system is free to change, and if that
#  spelling ever changes — a different generator, a compile wrapper, ccache with
#  `base_dir`, a build tree moved outside the checkout — the step keeps printing
#  "no warnings in first-party sources" forever with zero coverage and no signal
#  that it stopped working. A gate that cannot fail is indistinguishable from a
#  gate that passes, which is the failure mode INC-004 was: 1039 green checks
#  over a configuration nobody shipped.
#
#  So the classification here is STRUCTURAL rather than textual. Every
#  diagnostic's path is resolved — absolute taken as-is, relative resolved
#  against the directory the compiler ran in — and then asked one question:
#  does the resulting real path live under <root>/{src,tests,tools} without
#  passing through a `_deps` component? That answer does not depend on how the
#  path was spelled, which is the property the old expression lacked.
#
#  `--self-test` is the repository's existing answer to "prove the checker is
#  live before trusting its silence" (`check-docs.py --self-test`, and
#  `check-portability.py --compile-canary` for the same reason). It feeds the
#  classifier every path spelling this build could plausibly produce, plus the
#  vendored forms that must NOT count, and fails if any of them is misclassified.
#
#  Exit codes follow the sibling scripts: 0 clean · 1 first-party warnings found
#  (a real gate failure) · 2 the check itself could not run (self-test failure,
#  missing log) — deliberately not the 1 that means "the tree has warnings".
# ============================================================================

import argparse
import os
import re
import sys

# `path:line:col: [warning|error]: text`. The path is non-greedy and forbidden
# from containing a colon-digit run, so a Windows drive letter or a colon inside
# a message cannot be mistaken for the line number.
DIAGNOSTIC = re.compile(r"^(?P<path>.+?):(?P<line>\d+):(?P<col>\d+):\s+warning:")

FIRST_PARTY_DIRS = ("src", "tests", "tools")
VENDORED_COMPONENT = "_deps"


def classify(path: str, root: str, base: str) -> bool:
    """True when `path` names a first-party source file.

    `root` is the repository checkout; `base` is the directory the compiler ran
    in, which is what a relative diagnostic path is relative to (for Ninja that
    is the build directory). Both are resolved so that `..` segments, symlinks
    and a build tree nested inside the checkout all collapse to one answer.
    """
    resolved = path if os.path.isabs(path) else os.path.join(base, path)
    resolved = os.path.normpath(os.path.realpath(resolved))
    root = os.path.normpath(os.path.realpath(root))

    try:
        rel = os.path.relpath(resolved, root)
    except ValueError:          # different drive on Windows — cannot be ours
        return False

    parts = rel.split(os.sep)
    if parts and parts[0] == os.pardir:
        return False            # outside the checkout entirely
    # A dependency's own `src/` directory is the trap the old textual filter
    # needed a second `grep -v` for: `build/_deps/juce-src/…/src/…` starts with
    # `_deps` only after the build directory, so the test is "does ANY component
    # say _deps", not "does it start with one".
    if VENDORED_COMPONENT in parts:
        return False
    return bool(parts) and parts[0] in FIRST_PARTY_DIRS


def scan(log_path: str, root: str, base: str):
    try:
        with open(log_path, "r", errors="replace") as handle:
            lines = handle.read().splitlines()
    except OSError as exc:
        print(f"check-clang-warnings: cannot read {log_path}: {exc}", file=sys.stderr)
        return None, None

    first_party, other = [], []
    for line in lines:
        match = DIAGNOSTIC.match(line.strip())
        if match is None:
            continue
        (first_party if classify(match.group("path"), root, base) else other).append(line)
    return first_party, other


# --- self-test ---------------------------------------------------------------
# The cases are written against a synthetic tree so the expectations are exact.
# Every one of them is a spelling this build could produce, or one a plausible
# change to it would: the absolute form CMake emits today, the `../src/…` form a
# relative-path generator emits, the `./` form, and the two vendored shapes that
# must never count no matter how they are spelled.
SELF_TEST_CASES = [
    # (path as the compiler printed it, first-party?, label)
    ("/repo/src/dsp/AnabasisEngine.cpp",            True,  "absolute first-party (what this build emits today)"),
    ("/repo/tests/state_tests.cpp",                 True,  "absolute first-party under tests/"),
    ("/repo/tools/channel_probe.cpp",               True,  "absolute first-party under tools/"),
    ("../src/gui/PluginEditor.cpp",                 True,  "relative first-party from the build dir"),
    ("../../src/gui/PluginEditor.cpp",              False, "relative escaping the checkout"),
    ("src/dsp/ClipSat.h",                           False, "relative to the BUILD dir, so not our src/"),
    ("/repo/build/_deps/juce-src/modules/x.cpp",    False, "vendored, absolute"),
    ("_deps/juce-src/modules/juce_core/src/y.cpp",  False, "vendored whose OWN path contains src/"),
    ("../build/_deps/juce-src/modules/z.cpp",       False, "vendored, relative"),
    ("/elsewhere/src/other.cpp",                    False, "another checkout's src/"),
]


def self_test() -> int:
    import tempfile

    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        root = os.path.join(tmp, "repo")
        base = os.path.join(root, "build")
        os.makedirs(base)
        for name in FIRST_PARTY_DIRS:
            os.makedirs(os.path.join(root, name), exist_ok=True)

        for raw, expected, label in SELF_TEST_CASES:
            path = raw.replace("/repo", root, 1) if raw.startswith("/repo") else raw
            got = classify(path, root, base)
            if got != expected:
                failures += 1
                print(f"self-test FAIL: {label}: {path!r} -> {got}, want {expected}",
                      file=sys.stderr)

        # The line matcher itself, not just the path classifier: a diagnostic
        # must be recognised and a NON-diagnostic must not be, or the gate would
        # either miss real warnings or fail on prose that mentions "warning:".
        line_cases = [
            ("/repo/src/a.cpp:12:3: warning: unused variable 'x'", True,  "a real warning line"),
            ("/repo/src/a.cpp:12:3: error: no member named 'y'",   False, "an error is not this gate's business"),
            ("/repo/src/a.cpp:12:3: note: expanded from macro",    False, "a note"),
            ("cmake: -- warning: something happened",              False, "prose containing 'warning:'"),
            ("[52/91] Building CXX object src/a.cpp.o",            False, "a progress line"),
            # The GCC LTO lane emits this on EVERY link, so its treatment is
            # pinned rather than left incidental. Driver-level warnings carry no
            # source location at all, which is precisely why they must not be
            # gated: there is no file to attribute them to, and `lto-wrapper`'s
            # LTRANS-parallelism notice in particular is a property of the
            # machine, not of this tree. Measured, not assumed -- every GCC
            # diagnostic that DOES name first-party code was checked to carry
            # the `path:line:col:` form this matcher requires, including the
            # link-time `-Wodr` and `-Wlto-type-mismatch` pair that is the whole
            # reason the LTO lane exists.
            ("lto-wrapper: warning: using serial compilation of 5 LTRANS jobs",
                                                                   False, "LTO driver warning, no source location"),
            ("cc1plus: warning: command-line option is valid for C but not C++",
                                                                   False, "front-end driver warning, no source location"),
            ("/usr/bin/ld: warning: section has no contents",      False, "linker warning, no source location"),
        ]
        for raw, expected, label in line_cases:
            got = DIAGNOSTIC.match(raw.replace("/repo", root, 1).strip()) is not None
            if got != expected:
                failures += 1
                print(f"self-test FAIL: {label}: {raw!r} matched={got}, want {expected}",
                      file=sys.stderr)

    total = len(SELF_TEST_CASES) + len(line_cases)
    if failures:
        print(f"\ncheck-clang-warnings: {failures} of {total} self-test case(s) failed.",
              file=sys.stderr)
        return 2
    print(f"check-clang-warnings: self-test passed ({total} cases).")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", help="compiler output to scan")
    parser.add_argument("--root", default=os.getcwd(),
                        help="repository checkout root (default: cwd)")
    parser.add_argument("--build-dir", default=None,
                        help="directory the compiler ran in; relative diagnostic "
                             "paths resolve against it (default: --root)")
    # The script is named for the compiler it was written against, but three
    # lanes now feed it and two of them are GCC (`linux-lto-tests` runs g++ in
    # the pinned container; `linux`/`linux-lto-clang` run clang++). A failure
    # message that says "Clang emitted 4 warnings" underneath a g++ build is a
    # false attribution in the one output an operator reads first, so the name
    # is passed in rather than assumed. The default stays neutral: a caller that
    # does not say gets "the compiler", never the wrong one.
    parser.add_argument("--compiler", default="the compiler",
                        help="name of the compiler that produced the log, for "
                             "the failure message (default: 'the compiler')")
    parser.add_argument("--self-test", action="store_true",
                        help="prove the classifier is live, then exit")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    if not args.log:
        parser.error("--log is required unless --self-test is given")

    base = args.build_dir or args.root
    first_party, other = scan(args.log, args.root, base)
    if first_party is None:
        return 2

    if first_party:
        for line in first_party:
            print(line)
        print(f"::error::{args.compiler} emitted {len(first_party)} warning(s) in "
              f"first-party sources "
              f"({', '.join(d + '/' for d in FIRST_PARTY_DIRS)}).", file=sys.stderr)
        return 1

    # The dependency count is REPORTED, never gated on: a blanket -Werror over
    # JUCE's own module sources would be switched off at the first bump. Printing
    # it is what tells an operator the log was actually parsed — a zero here
    # alongside a large build is the shape that says "look at the log", which the
    # silent `grep` could not say.
    print(f"check-clang-warnings: no first-party warnings "
          f"({len(other)} in vendored/other paths, not gated).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
