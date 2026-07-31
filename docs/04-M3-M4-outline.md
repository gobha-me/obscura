# M3 / M4 — Generator, solver, and the gathering

Deliberately at outline depth. Both milestones are upside; if M3 never lands you
still have a finished, shippable game with five authored cases. **The generator is
upside, not foundation.**

## M3 — Generator and solver

> **Revised by the tile grammar — read `10-tile-grammar.md` first.** Layout
> validity is no longer M3's problem: socket constraints make an illegal ship
> unrepresentable, and route cost is computable, so M3 is one hard problem
> (deduction solvability) plus arithmetic. The topology bullet below is
> superseded by the tile grammar.

**Gate:** *Can the solver reject unsolvable seeds at acceptable cost?*

Order is non-negotiable: **truth first, evidence second, redaction third.**
Reversing any two produces puzzles that are unfair in ways that are very hard to
debug.

- Hull topology from `STREAM_HULL`: 12–24 compartments (renderer caps at 15 until
  the embedder gets smarter — DR-05), graph then embedded onto the three-band deck
  plan. Every edge must be drawable horizontally within a band or vertically
  between bands. **No diagonals** — unreadable at one cell of line weight. The
  renderer constrains the generator, deliberately.
- Plausibility constraints: engineering aft and low, bridge forward, airlocks on
  the hull boundary, no berth opening directly into a hold.
- Ground truth from `STREAM_INCIDENT`: 6–10 actors with role and timeline, one
  root-cause archetype, propagation rules advancing over discrete steps and
  mutating damage states. A fully specified causal object *before* any evidence.
- Evidence derivation from `STREAM_EVIDENCE`: each timeline step and damage
  transition emits candidates, gated by kind and instrument. Veracity assigned
  here and never surfaced.
- Redaction from `STREAM_REDACT`: remove until solvable-but-not-trivial, with one
  hard invariant — every commit in the solution key keeps at least one served
  chain under the published loadout.
- **Solver**: must produce a chain table like Appendix A.6 mechanically, and
  *prove* solvability before a seed is served. Measure the rejection rate and the
  per-seed time budget; do not assume either.
- Optional: T-D4 shared-memory transfer for large plates. Opt-in, falls back to
  base64 with an event.

## M4 — The gathering
**Gate:** *Is the verification path actually unfakeable?*

- Daily seed: `splitmix64(hash(utc_date_yyyymmdd) ^ SALT)`, computed once at
  launch and then treated as data.
- Server-side re-simulation of submitted replays; compare final-state hash.
  This is the whole anti-cheat, and it ships no client-side anti-cheat at all.
- Leaderboard compares reasoning, not unlocks: fixed published loadout for daily
  seeds.
- T-H6: sanitisation boundary review for player-supplied names. Escape stripping
  stays in the **renderer**, never the driver — that split is the injection
  defence. Add a test with a hostile name.
- Public release. Kitty-only shrinks the audience to a rounding error; that is
  accepted and deliberate.
