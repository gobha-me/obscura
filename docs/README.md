# Handoff: OBSCURA — deduction roguelike on termforge

## Overview
OBSCURA is a seeded deduction roguelike aboard a derelict, built in C++23 on
**termforge** (https://github.com/gobha-me/termforge), Kitty graphics protocol only.
Its thesis: **rendering fidelity is the game state** — a compartment renders as glyph
noise until the player deduces what happened in it, at which point it resolves
permanently into a 4-colour art plate.

This bundle is the implementation package. The canonical spec is
`OBSCURA-design.html` (open in a browser; print to PDF for a paper copy).
The markdown files here are the executable plan.

## Fidelity
**This is an engineering design specification, not a UI mock.** There are no
pixel comps to recreate: the deliverable is a terminal program, and the visual
spec is expressed as *cell-accurate region tables plus rendered figures* in §8 of
the design doc and in `09-screens.md`. Treat the region tables as normative
(origins, extents, minimum grid 120x40) and the ASCII figures as the intended
appearance at those coordinates. The HTML design doc itself is a **document**,
not an app — nothing in it is meant to be ported into the game.

## Read in this order
| File | What it is |
|---|---|
| `OBSCURA-design.html` | Canonical design spec, rev 2.0. Read fully before writing code. |
| `../CLAUDE.md` | At the repo root. Hard rules, build commands, conventions, upstream status. |
| `05-termforge-tickets.md` | Epics A–H with ready-to-file ticket bodies. |
| `01-M0-render-spike.md` | The only milestone that starts today. Library work in termforge. |
| `02-M1-fun-test.md` | First playable. One authored case. |
| `03-M2-content-consequence.md` | Five cases, replay, RtAudio. |
| `04-M3-M4-outline.md` | Generator/solver and the daily seed, at outline depth by design. |
| `06-case-001-cold-lantern.md` | The authored M1 case: truth, evidence, redaction, solvability proof. |
| `07-decision-records.md` | 14 decision records. Four are decided; the rest are open with tradeoffs. |
| `08-determinism.md` | The bit-exactness contract and its CI gates. |
| `09-screens.md` | Cell-accurate layouts for all five modes. |
| `10-tile-grammar.md` | Tile sockets, ship classes, layer composition. Revises §6.1/§7.6 and M3. |

## Two tracks, one dependency order
**The single most important thing in this package:** three of the capabilities the
game needs do not exist in termforge yet — a layer/z-band API, incremental image
frame edits (the dissolve), and the Kitty keyboard protocol with release events
(the commit gesture). So:

- **T-series = library work in termforge.** M0 is almost entirely this.
- **M-series = game work in a new OBSCURA repo.**
- Do not create the OBSCURA repository until the M0 gate passes. M0 lives in
  termforge's own `examples/` where the test harness already is.

You are authorised to **open feature-request tickets against termforge** for the
T-series. Follow the filing protocol in `05-termforge-tickets.md`: search before
filing, one landable change per ticket, acceptance criteria a test can satisfy.

## Stack (fixed, do not substitute)
- C++23, GCC 13+ / Clang 17+. Both compilers must build clean and pass.
- CMake >= 3.28. **No package manager** — `FetchContent` only.
- Catch2 v3 via FetchContent. termforge as `termforge::lib`.
- RtAudio for audio, **optional**, behind `OBSCURA_AUDIO`. See `03-M2`.
- Kitty protocol only. No sixel, no degraded mode, no undo, no save-scum.

## Definition of done for the whole package
A player runs `obscura --seed=...` in a Kitty-protocol terminal, boards the
derelict *Cold Lantern*, surveys compartments against a suit-charge budget,
reads evidence in a scrolling log with real inline images, commits three
assertions, and watches three compartments dissolve into art in 400 ms —
and the whole session replays bit-identically from a 2 KB file.

---

# Corrections (2026-07-31)

This bundle was authored against **termforge v0.1.7**. Three of its claims are now
stale. The *design* remains canonical; the *upstream status* does not.

### 1. termforge is at v0.6.3, not v0.1.7
The pin in this repo is **v0.6.3**. The delta matters — `on_start`/`on_stop`
hooks, `image_cell_extent()`, and the entire Kitty keyboard protocol all landed
in between.

### 2. The Kitty keyboard protocol has LANDED
The bundle states termforge "has no keyboard-protocol negotiation" and that
T-E1/T-E2 must be filed. **Both shipped** (termforge#60):
`KeyAction{Press,Repeat,Release}`, `KeyEvent::action`,
`KeyboardMode{Legacy,Disambiguate,Enhanced}`, `Capabilities::kitty_keyboard`,
push-on-setup / pop-on-teardown including the crash path. Tested in
`test/04input` and `test/31keyboard` (real pty).

**This retires the largest technical risk to the commit gesture.** Only mid-session
*loss* detection (T-E3) remains unfiled.

### 3. Issue numbers, and the current T-series mapping
The bundle says issues "currently run to #102" and that #91/#97/#100/#102 are the
four that exist. Issues now run past **#143**, and #97/#100/#102 are **closed**.

| T | termforge issue | State |
|---|---|---|
| T-A1 | #91 | open |
| T-A2 | #97 | ✅ closed, landed (`on_start`/`on_stop`) |
| T-A3 | #91 | floor half in #91; resize half raised as a comment |
| T-A4 | #100 | ✅ closed, landed (`image_cell_extent()`) |
| T-B1 | #114 | open — **needs FIVE planes, not the four proposed** |
| T-B2 | #142 | filed 2026-07-31 |
| T-C1 | #140 | filed 2026-07-31 |
| T-C2 | #116 | open — needs per-frame gaps + zero-gap bases |
| T-C4 | #141 | filed 2026-07-31 |
| T-D1 | #113 | open |
| T-D2 | #112, #109 | open |
| T-D4 | #111 | open (optional, M3) |
| T-E1, T-E2 | #60 | ✅ closed, **LANDED** |
| T-F1 | #120 | open |
| T-F2 | #119, #118 | open |
| T-H4 | #139 | filed 2026-07-31 — **do first** |
| T-H5 | #143 | filed 2026-07-31 |

Deliberately unfiled per the *no speculative tickets* rule, until their milestone
approaches: T-D3, T-D5, T-E3, T-F3, T-G1/G2/G3, T-H6.

### 4. On "do not create the OBSCURA repository until the M0 gate passes"
This repository was created at the **design phase**, ahead of that instruction, as
the planning home. The dependency order is unchanged: **M0 code still lands in
termforge's `examples/`**, where the test harness and both-compiler CI already
are. M0 is tracked here as issues, not built here. See DR-10 in
`07-decision-records.md` — the repo is private through the M0 gate so a failure
can still be quiet.
