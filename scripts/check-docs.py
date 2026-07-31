#!/usr/bin/env python3
"""Structural lint for the Anabasis documentation set.

Three checks, all mechanical and deterministic. Each exists because the defect it
catches shipped at least once in this repository and was invisible in the source
diff that introduced it:

  1. TABLE INTEGRITY -- a blockquote or paragraph inserted in the middle of a
     GitHub-Flavored Markdown table terminates it. The rows after the intrusion
     render as pipe-separated text with no header, outside the table their own
     header governs. This happened to THREADING_POLICY.md's permitted-path table:
     three of the seven binding cross-thread rules stopped being rules. A table
     cannot resume after an intervening block in GFM, so any run of lines
     starting with `|` whose second line is not a separator (`|---|`) is either
     an orphaned fragment or a headerless table.

  2. RELATIVE LINKS -- a moved or renamed file silently breaks every pointer to
     it. The docs are a navigation system (SOURCE_OF_TRUTH.md, REPOSITORY_MAP.md,
     the ADR index); a dead link there is a reader who does not reach a binding
     record.

  3. BLOCKQUOTE LAZY CONTINUATION -- an unquoted line directly after a `>` line
     is absorbed into the quote by Markdown's lazy-continuation rule. This
     happened to ADR-0011: two sentences of binding contract rendered as part of a
     historical correction note, which a reader could reasonably skip.

Deliberately NOT checked here: whether each ADR-prescribed policy block matches
the enacted policy text. That comparison is real and is run by hand on every
documentation pass, but it has known cosmetic artefacts -- the policy bolds each
invariant's opening sentence as a headline and stamps a dated attribution that the
prescribing ADR does not carry (see ADR-0003's "scope of verbatim" note). Encoding
those exceptions as an allowlist would make the script assert more than it can
check, which is the failure mode constraint C7 exists to prevent.

Usage:  scripts/check-docs.py [path ...]     (default: the repository root)
Exit:   0 = clean, 1 = findings printed to stderr.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

SEPARATOR = re.compile(r"^\|[\s:|-]+\|$")
INLINE_LINK = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
# A line that Markdown will NOT absorb into a preceding blockquote: another quote
# line, or a block-level construct that interrupts the paragraph.
NOT_ABSORBED = ("|", "#", "-", "*", "`", ">", "+", "=", "_")
SKIP_DIRS = {".git", "build", "node_modules", "JUCE"}


def markdown_files(roots: list[Path]) -> list[Path]:
    out: list[Path] = []
    for root in roots:
        if root.is_file() and root.suffix == ".md":
            out.append(root)
            continue
        for path in sorted(root.rglob("*.md")):
            if SKIP_DIRS.isdisjoint(path.parts):
                out.append(path)
    return out


def check_tables(path: Path, lines: list[str]) -> list[str]:
    """Every run of pipe-prefixed lines must open with a header + separator pair."""
    findings, run = [], []
    for lineno, line in enumerate(lines + [""], start=1):
        if line.startswith("|"):
            run.append((lineno, line))
            continue
        if run:
            if len(run) < 2 or not SEPARATOR.match(run[1][1].strip()):
                findings.append(
                    f"{path}:{run[0][0]}: table fragment with no header/separator "
                    f"({len(run)} pipe line(s)) -- a block was inserted mid-table, "
                    f"or the separator row is missing"
                )
            run = []
    return findings


def check_links(path: Path, text: str) -> list[str]:
    findings = []
    for match in INLINE_LINK.finditer(text):
        target = match.group(1).split("#", 1)[0].strip()
        if not target or target.startswith(("http://", "https://", "mailto:")):
            continue
        if not (path.parent / target).resolve().exists():
            line = text.count("\n", 0, match.start()) + 1
            findings.append(f"{path}:{line}: broken relative link -> {target}")
    return findings


def check_lazy_continuation(path: Path, lines: list[str]) -> list[str]:
    findings = []
    for i in range(len(lines) - 1):
        nxt = lines[i + 1].lstrip()
        if lines[i].lstrip().startswith(">") and nxt and not nxt.startswith(NOT_ABSORBED):
            findings.append(
                f"{path}:{i + 2}: line is absorbed into the preceding blockquote "
                f"by lazy continuation -- insert a blank line or quote it"
            )
    return findings


def main(argv: list[str]) -> int:
    roots = [Path(a) for a in argv[1:]] or [Path(__file__).resolve().parent.parent]
    for root in roots:
        if not root.exists():
            print(f"check-docs: no such path: {root}", file=sys.stderr)
            return 1

    files = markdown_files(roots)
    findings: list[str] = []
    for path in files:
        text = path.read_text(encoding="utf-8")
        lines = text.split("\n")
        findings += check_tables(path, lines)
        findings += check_links(path, text)
        findings += check_lazy_continuation(path, lines)

    if findings:
        for finding in findings:
            print(finding, file=sys.stderr)
        print(
            f"\ncheck-docs: {len(findings)} finding(s) across {len(files)} file(s).",
            file=sys.stderr,
        )
        return 1

    print(f"check-docs: {len(files)} file(s) clean.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
