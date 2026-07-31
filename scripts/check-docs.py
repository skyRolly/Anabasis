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
     is absorbed into the quote by CommonMark's lazy-continuation rule. This
     happened to ADR-0011: two sentences of binding contract rendered as part of a
     historical correction note, which a reader could reasonably skip.

FALSE POSITIVES ARE THE FAILURE MODE THAT MATTERS. A lint that invents findings
gets ignored, and then the real ones are lost with it -- so each check is scoped
to what it can actually prove:

  * Fenced code blocks are excluded from all three checks. A document that shows
    table syntax, a link, or quote syntax as an *example* is not making a claim
    about its own structure. (This file's own docstring would otherwise trip it.)
  * Lazy continuation is only reported when CommonMark actually applies it: to
    *paragraph continuation text*. A quote that ends with a blank `>` line has
    closed its paragraph, and a line that starts a new block -- heading, fence,
    list, table row, thematic break, HTML -- interrupts rather than continues.
    `interrupts_paragraph()` encodes those cases, including CommonMark's rule
    that an ordered list interrupts a paragraph only when it starts at 1.
  * Link destinations are parsed, not string-sliced: `[t](path "Title")` and
    `[t](<path with spaces>)` are both valid and neither is a broken link.

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
INLINE_LINK = re.compile(r"\[[^\]]*\]\(([^)]*)\)")
FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
SKIP_DIRS = {".git", "build", "node_modules", "JUCE"}

# Blocks that interrupt a paragraph, and therefore are NOT swallowed by a
# preceding blockquote's lazy continuation (CommonMark 0.31 §4, §5.1, §6.9).
_INTERRUPTERS = (
    re.compile(r"^\s{0,3}#{1,6}(\s|$)"),            # ATX heading (space required)
    re.compile(r"^\s{0,3}(`{3,}|~{3,})"),           # fenced code block
    re.compile(r"^\s{0,3}([-*_])(\s*\1){2,}\s*$"),  # thematic break
    re.compile(r"^\s{0,3}>"),                        # another blockquote line
    re.compile(r"^\s{0,3}[-*+](\s|$)"),              # bullet list item
    re.compile(r"^\s{0,3}1[.)](\s|$)"),              # ordered list — only "1" interrupts
    re.compile(r"^\s{0,3}\|"),                       # GFM table row
    re.compile(r"^\s{0,3}<"),                        # HTML block
    re.compile(r"^\s{0,3}=+\s*$"),                   # setext underline
)


def interrupts_paragraph(line: str) -> bool:
    """Whether `line` starts a block instead of continuing a paragraph.

    Indentation is stripped before matching. CommonMark measures a block's
    indent against its *container's* content column, and a line-based lint has
    no container stack -- inside a numbered ADR item, a bullet sits five columns
    in and is still a bullet. Anchoring at column 0 flagged ~30 such lines as
    absorbed when they are not. Stripping trades a few false negatives (a deeply
    indented line that genuinely is continuation text) for no false positives,
    which is the correct direction for a lint nobody is obliged to run.
    """
    return any(rx.match(line.lstrip()) for rx in _INTERRUPTERS)


def fence_mask(lines: list[str]) -> list[bool]:
    """True for every line that is inside (or delimits) a fenced code block."""
    mask, opener = [False] * len(lines), None
    for i, line in enumerate(lines):
        match = FENCE.match(line)
        if opener is None:
            if match:
                opener, mask[i] = match.group(1)[0], True
            continue
        mask[i] = True                      # inside the fence, including its closer
        if match and match.group(1)[0] == opener:
            opener = None
    return mask


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


def link_destination(raw: str) -> str | None:
    """The path part of a link destination, or None if there is nothing to check.

    Handles `<angle-bracketed>` destinations and the optional title that may
    follow the destination in double quotes, single quotes or parentheses.
    """
    raw = raw.strip()
    if not raw:
        return None
    if raw.startswith("<"):
        end = raw.find(">")
        return raw[1:end] if end != -1 else raw[1:]
    dest = raw.split(None, 1)[0] if re.search(r'\s+["\'(]', raw) else raw
    return dest.strip()


def check_tables(path: Path, lines: list[str], fenced: list[bool]) -> list[str]:
    """Every run of pipe-prefixed lines must open with a header + separator pair."""
    findings, run = [], []

    def close(run: list[tuple[int, str]]) -> None:
        if len(run) < 2 or not SEPARATOR.match(run[1][1].strip()):
            findings.append(
                f"{path}:{run[0][0]}: table fragment with no header/separator "
                f"({len(run)} pipe line(s)) -- a block was inserted mid-table, "
                f"or the separator row is missing"
            )

    for i, line in enumerate(lines):
        if line.startswith("|") and not fenced[i]:
            run.append((i + 1, line))
        elif run:
            close(run)
            run = []
    if run:
        close(run)
    return findings


def check_links(path: Path, lines: list[str], fenced: list[bool]) -> list[str]:
    findings = []
    for i, line in enumerate(lines):
        if fenced[i]:
            continue
        for match in INLINE_LINK.finditer(line):
            dest = link_destination(match.group(1))
            if dest is None:
                continue
            target = dest.split("#", 1)[0].strip()
            if not target or target.startswith(("http://", "https://", "mailto:")):
                continue
            if not (path.parent / target).resolve().exists():
                findings.append(f"{path}:{i + 1}: broken relative link -> {target}")
    return findings


def check_lazy_continuation(path: Path, lines: list[str], fenced: list[bool]) -> list[str]:
    findings = []
    for i in range(len(lines) - 1):
        if fenced[i] or fenced[i + 1]:
            continue
        quote = lines[i].lstrip()
        if not quote.startswith(">"):
            continue
        if not quote.lstrip(">").strip():
            continue                        # blank quote line: the paragraph is closed
        nxt = lines[i + 1]
        if not nxt.strip() or interrupts_paragraph(nxt):
            continue
        findings.append(
            f"{path}:{i + 2}: line is absorbed into the preceding blockquote "
            f"by lazy continuation -- insert a blank line or quote it"
        )
    return findings


def self_test() -> int:
    """Assert the checks fire on real defects and stay silent on valid markup.

    Every case below is one that actually reached review: the first shipped
    revision of this script reported all five FP cases as findings. A lint whose
    only evidence is "it returns clean on our tree" proves nothing -- both
    directions have to be pinned.
    """
    doc = Path("x.md")
    cases: list[tuple[str, int, object, list[str]]] = [
        ("table syntax inside a fence", 0, check_tables, ["```", "| not a table", "```"]),
        ("blank quote line ends the paragraph", 0, check_lazy_continuation,
         ["> quote", ">", "Normal paragraph."]),
        ("ordered list at 1 interrupts", 0, check_lazy_continuation, ["> quote", "1. item"]),
        ("quote syntax inside a fence", 0, check_lazy_continuation,
         ["```", "> quote", "text", "```"]),
        ("link inside a fence", 0, check_links, ["```", "[a](nope.md)", "```"]),
        ("block inserted mid-table", 1, check_tables,
         ["| A | B |", "|---|---|", "| 1 | 2 |", "> intruder", "| 3 | 4 |"]),
        ("genuine lazy continuation", 1, check_lazy_continuation, ["> quote", "absorbed line"]),
        ("table with no separator row", 1, check_tables, ["| a | b |", "| c | d |"]),
        ("ordered list not at 1 is absorbed", 1, check_lazy_continuation, ["> quote", "2. item"]),
    ]
    failures = 0
    for label, expected, check, lines in cases:
        got = len(check(doc, lines, fence_mask(lines)))  # type: ignore[operator]
        if got != expected:
            failures += 1
            print(f"self-test FAIL: {label}: expected {expected}, got {got}", file=sys.stderr)

    for raw, want in [
        ('docs/DESIGN.md "T"', "docs/DESIGN.md"),   # double-quoted title
        ("p.md 'T'", "p.md"),                        # single-quoted title
        ("p.md (T)", "p.md"),                        # parenthesised title
        ("<a b.md>", "a b.md"),                      # angle-bracketed destination
        ("p.md", "p.md"),
    ]:
        got_dest = link_destination(raw)
        if got_dest != want:
            failures += 1
            print(
                f"self-test FAIL: link_destination({raw!r}) -> {got_dest!r}, want {want!r}",
                file=sys.stderr,
            )

    if failures:
        print(f"\ncheck-docs: {failures} self-test failure(s).", file=sys.stderr)
        return 1
    print(f"check-docs: self-test passed ({len(cases) + 5} cases).")
    return 0


def main(argv: list[str]) -> int:
    if "--self-test" in argv[1:]:
        return self_test()

    roots = [Path(a) for a in argv[1:]] or [Path(__file__).resolve().parent.parent]
    for root in roots:
        if not root.exists():
            print(f"check-docs: no such path: {root}", file=sys.stderr)
            return 1

    files = markdown_files(roots)
    findings: list[str] = []
    for path in files:
        lines = path.read_text(encoding="utf-8").split("\n")
        fenced = fence_mask(lines)
        findings += check_tables(path, lines, fenced)
        findings += check_links(path, lines, fenced)
        findings += check_lazy_continuation(path, lines, fenced)

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
