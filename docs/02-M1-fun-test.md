# M1 — The fun test

**Where:** a new `obscura/` repository (layout in the spec §2.4). Start it only
after the M0 gate passes.
**Scope:** one hand-authored case, no generator, no meta progression, no
leaderboard, fixed loadout. Playable start to finish in 30–45 minutes.
**Gate:** *Is it fun?* Hand it to three people who did not build it.
**Hard rule:** building a constraint solver before this gate passes is the single
most likely way this project dies.

## Tickets
T-A3 (minimum grid size + pause-to-modal on shrink), T-D2 (quota accounting),
T-D3 (asset pack residency), T-E1/T-E2/T-E3 (Kitty keyboard protocol),
T-G1 (`AudioSink` interface + `NullSink`, stdlib-only).

> **Correction (2026-07-31).** T-E1 and T-E2 **already landed** (termforge#60).
> `KeyAction{Press,Repeat,Release}`, `KeyEvent::action`, `KeyboardMode`,
> `Capabilities::kitty_keyboard`, push-on-setup / pop-on-teardown including the
> crash path — all present in v0.6.3 and tested in `test/04input` and
> `test/31keyboard`. T-E3 subsequently landed in v0.57.24: live loss is now an
> observable, replayable transition rather than a silent press-only downgrade.

## Tasks
### Repo and gates
- [ ] Repo per spec §2.4. `CLAUDE.md` from this bundle at the root.
- [ ] CMake: C++23, FetchContent termforge + Catch2 v3, presets for
      default / clang / asan / tsan / ubsan / release.
- [ ] `tools/lint/sim_purity.sh` wired as a CTest case **before** `src/world/`
      has content. It is cheap to satisfy from day one and expensive to retrofit.
- [ ] Refuse-to-start contract (spec §2.3): Kitty graphics with below-background
      images, keyboard protocol with event types, >= 120x40 grid, known cell
      geometry >= 6x12 px. Exit 78 in cooked mode with the probe response.
      Never enter alt-screen before the floor is verified.

### World
- [ ] Types from spec §5 verbatim: `Compartment`, `Fact`, `Evidence`, `Actor`,
      `ActorStep`, `Commit`, `Truth`, and the enums.
- [ ] Case 001 *Cold Lantern* as `cases/case001_cold_lantern.cpp`, C++ constexpr
      data (DR-13): 12 compartments, 15 edges, 7 actors, 9 timesteps, 16 served
      evidence items. Data is in `06-case-001-cold-lantern.md` — transcribe it,
      do not re-invent it.
- [ ] Commit verification: scan the asserted actor's timeline for a step matching
      (where, what) at any time. Time is not part of a commit in M1 (DR-14).
- [ ] Redaction invariant test: every commit in the solution key has at least one
      served chain under the published loadout.

### Render
- [ ] Glyph substrate: seeded from `STREAM_GLYPH`; three character classes —
      shade blocks for mass, box-drawing debris for unresolved structure,
      half-legible archetype manifest fragments. Corruption ratio scales with
      distance from the cursor, so the next compartment is *almost* known.
- [ ] SHIP mode at the region table in `09-screens.md`: three bands of 22x9
      compartment boxes at columns 0/24/48/72/96, trunk gutters, soot line,
      ledger strip, console bezel. Fixed reference grid; letterbox on larger
      terminals; never reflow.
- [ ] Survey firm-up (UNKNOWN → SURVEYED): wireframe plus damage tint in the cell
      backgrounds, evidence markers.
- [ ] Resolve dissolve, ported from the M0 spike.
- [ ] **Evidence log with real inline images** (Unicode placeholders, already
      supported by termforge). Build this in M1, not M2 — it is the screenshot
      that sells the project.

### Input
- [ ] Key map per spec §9.
- [ ] Commit gesture: `Space` **press** opens AIM, **release** commits, `Esc`
      aborts free even while held. A release with no matching press aborts —
      fail toward the state that costs the player nothing. Protocol loss
      mid-session pauses to a modal and refuses to continue.
- [ ] AIM overlay, 72x20 at (24,10). Supporting/contradicting panel computed by
      fact intersection over the player's own log. **Test that it cannot reach
      `Truth` or `Veracity`.**

### Economy and session
- [ ] Budget as one `constexpr` table (spec §4.3): start 120 SC; move 1 (3 if
      breached); survey 8; examine 3; re-read 2; abort 0; batch correct +15;
      batch wrong -20 and **never disclose which of the three failed**.
- [ ] Batch of three, pending slots, resolve confirmation modal.
- [ ] DEBRIEF with reconstruction percentage, charge spent by category, seed, and
      the plate gallery.
- [ ] One recorded replay in `test/corpus/` with its final-state hash.

## Exit criteria
1. Case 001 is completable **and** losable.
2. Three external players finish a session unaided; session length 30–45 min.
3. Determinism corpus green across both compilers, -O0/-O2, ASan/UBSan.
4. Sim purity lint green. UI firewall test green.
5. Bandwidth assertions still hold with a full session's traffic.

## Gate ritual
Watch someone lose. If losing produces "one more run" rather than a shrug, the
loop works. If they cannot articulate *why* a batch failed, the evidence chains
are too thin — tune Appendix A's cost table before touching the design.
