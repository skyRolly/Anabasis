# Anabasis — internal testing guide

**For internal testers and pre-release validation users.**

Anabasis is a **pre-release build**. It is not a released product, it is not on sale, and this
document is not a customer-support channel — it is the working guide for the people testing the
build, and the definition of what a usable test report contains.

`DEVELOPMENT_BRIEF.md` §14.2 names this file as a member of the **Internal / testing**
documentation class, whose rule is that it "restates the legal class, never diverges from it".
Section 1 is where that rule bites, and it is the reason this file reads shorter than the
sibling product's.

---

## 1. What this document can and cannot tell you

**Anabasis has no approved licence, EULA or privacy statement, and none is written.** That is a
recorded state, not an oversight: `OPEN_QUESTIONS.md` **OQ-002** (the JUCE licence tier) and
**OQ-009** (owner and support metadata) are open, and `docs/HANDOVER.md` records that the
owner-legal document set was deliberately not produced while they are. The sibling product's
equivalent of this file restates *its* legal documents; there is nothing here to restate.

So this file states only what the repository can evidence:

- **Anabasis is closed-source.** © 2026 RollyTech. This repository's technical documentation is
  not a source-code licence, and a test build carries no source-code rights.
- **Terms come from the owner, not from here.** If you were given a build under conditions —
  confidentiality, an embargo, a redistribution rule — those came from the owner directly. This
  document neither grants nor restates them, and its silence is not permission.
- **Treat every build as pre-release.** Every automated gate passing means the build is *ready
  to audition*, not final. Do not put it on production work you cannot afford to redo. Confirmed
  limitations and their workarounds are in
  [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md).

## 2. Before you report

1. **Check [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md).** Several behaviours that look like
   defects are documented limitations with a stated workaround.
2. **Check [`CHANGELOG.md`](CHANGELOG.md).** The behaviour may have changed deliberately in the
   build you are on.
3. **Check the manual** — [`docs/user/USER_MANUAL.md`](docs/user/USER_MANUAL.md), and
   [`docs/user/INSTALLATION.md`](docs/user/INSTALLATION.md) for install and rescan problems,
   which are the most common "it does not load" cause.

## 3. Where to report

The **GitHub issue tracker** for this repository, using the *Test report — bug* form
(`.github/ISSUE_TEMPLATE/bug_report.yml`). That form asks for exactly the fields a report needs
to be reproducible, and it is the only reporting channel this repository defines.

⚠ **The tracker is public.** Attach nothing you are not willing to publish. If you were given a
private channel for this testing round, use that for anything else.

**There is no log file.** Anabasis writes no diagnostic output of any kind — no crash log, no
telemetry, no console spew — so the fields in the form are not bureaucracy: they are the whole
of what makes a report reproducible.

## 4. What a usable report contains

The bug form asks for these; they are listed here so you can gather them before you start.

| Field | Why it decides whether the report is actionable |
|---|---|
| **What happens / what you expected** | Separates a defect from a misunderstanding of the design. |
| **Steps to reproduce** | From a *freshly inserted instance on a new track* — a report that only reproduces inside your session usually reproduces because of your session. |
| **Version and build number** | Both, from the **About** box (click the ANABASIS wordmark). The build number identifies the exact CI run. |
| **Host + version, OS + version** | `docs/architecture/COMPATIBILITY_MATRIX.md` tracks hosts individually; a defect is a host row until proven otherwise. |
| **Format** (VST3 / AU / Standalone) | The format wrappers are separate code paths. |
| **Sample rate and buffer size** | Several classes of defect are rate- or block-size-specific by construction. |

Two more that are worth volunteering because they have each decided a real investigation:

- **Heard or read?** Whether you *heard* the problem or *read it off a meter* — the plugin's own
  meters and the host's measure different points.
- **Where the build came from** — a CI artifact zip, an installer, or a local build.

## 5. What happens next

Reports are triaged into [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) when confirmed, with the
evidence that confirmed them and what the probes exclude. A report that cannot be reproduced is
recorded there too, with the environment detail still needed — an unreproduced report is a real
state, not a rejected one. KI-009 is the worked example: a field report that survived a
six-configuration headless battery, and whose eventual root cause was in the bus layout the
battery could not reach.
