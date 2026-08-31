# Determinism contract and its gates

Pillar 2 is "deterministic to the bit". The leaderboard, the replay format and the
regression suite all sit on it, so it is enforced by CI rather than by intention.

## Rules
- Seed is `uint64_t`. Daily seed `splitmix64(hash(utc_date_yyyymmdd) ^ SALT)`,
  computed once at launch, then treated as data.
- **Named streams only:** `rng(seed, stream_id)`. splitmix64 to derive streams,
  PCG-XSH-RR for the streams themselves — both integer-only with cheap
  independent streams. Adding a subsystem later must not perturb existing ones,
  so old replays stay valid across releases.

  | ID | Stream | Draws |
  |---|---|---|
  | 1 | `STREAM_HULL` | compartment count, archetypes, edges, grid embedding |
  | 2 | `STREAM_CREW` | actor count, roles, names, initial stations |
  | 3 | `STREAM_INCIDENT` | archetype, origin, propagation, actor decisions |
  | 4 | `STREAM_EVIDENCE` | derivation, kinds, instrument gating, veracity |
  | 5 | `STREAM_REDACT` | redaction candidate ordering |
  | 6 | `STREAM_FLAVOR` | non-load-bearing text; never consulted by the solver |
  | 7 | `STREAM_GLYPH` | glyph-noise substrate, so screenshots of a seed match |

- **No floating-point in simulation state.** Integers, or Q16.16 with explicit
  rounding. Floats in the render layer only, and render never feeds back into sim.
- **No wall-clock, no `random_device`.** The tick is cosmetic: sim advances only
  on discrete input events. `on_tick(dt)` drives the dissolve, cursor blink and
  log scroll — nothing that touches sim state.
- **No `unordered_*` iteration in order-sensitive paths.** Vectors or sorted flat
  maps with explicit comparators; dense integer IDs assigned in generation order;
  every sort's total order ends with the ID as tiebreak.

## Replay format
```
struct ReplayEvent { uint32_t seq; uint16_t key; uint8_t mods; uint8_t type; };  // 8 B
struct Replay {
  uint32_t magic, version;
  uint64_t seed;
  uint32_t build_id;        // asset pack manifest hash, truncated
  uint32_t event_count;
  ReplayEvent events[];     // ~250 events -> ~2 KB
  uint64_t final_state_hash;// FNV-1a 64 over the canonical serialisation
};
```
Wall-clock timing is deliberately absent: a session replays identically on a
loaded machine. `seq` is the only sim-visible ordering. The server re-simulates
and compares the hash, which makes the leaderboard unfakeable with no client-side
anti-cheat at all. The same file is the bug-report format and the regression
fixture — bug reports arrive as playable artifacts.

## Gates
| Gate | Mechanism |
|---|---|
| Sim purity | `tools/lint/sim_purity.sh` as a CTest case over `src/world/` + `src/core/sim/`. Fails on `float`, `double`, `std::chrono`, `random_device`, `rand(`, `time(`, `unordered_`, or any include outside an allow-list. Crude, greppable, effective. |
| Golden corpus | `test/corpus/*.replay` — one per authored case plus every bug report ever filed. Each replays and must match its recorded final-state hash. |
| Cross-configuration | The corpus must produce **identical** hashes under GCC and Clang, at -O0 and -O2, and under ASan/UBSan. A divergence is undefined behaviour, found cheaply. |
| Audio independence | Corpus run with audio compiled out and with `NullSink`; hashes unchanged. Prevents audio ever becoming load-bearing. |
| Stream isolation | A test registers a synthetic subsystem stream and asserts the existing corpus still matches — the property that keeps old replays valid across releases. |
| Bandwidth | Corpus replayed against T-H4's meter; §7.7's limits are assertions: idle frame <= 2 KB, dissolve <= 40 KB, sustained <= 250 KB/min. |
| UI firewall | The AIM overlay's supporting/contradicting panel never reads `Veracity` or `Truth`. Enforced by making them unreachable from the UI layer *and* by a test. |
| Redaction invariant | For every case, every commit in the solution key has at least one served chain under the published loadout. |

> **Correction (2026-07-31).** The bandwidth gate's "T-H4's meter" is now
> [gobha-me/termforge#139](https://github.com/gobha-me/termforge/issues/139).
>
> **Correction (2026-08-04).** The meter **landed** — #139 shipped in termforge
> v0.6.8, and this repo pins v0.7.1 as of today. Read it off the driver as
> `last_frame_bytes()` / `total_bytes()`, from inside an `App` subclass.
>
> **The idle-frame limit in the Bandwidth row is not assertable yet, and writing
> it today would give you a permanently red suite.** Measured through OBSCURA's
> own `on_render` offline, a full 80x24 repaint costs **16,344 bytes** — eight
> times the 2 KB figure. The cause is not our code: the renderer calls the driver
> once per changed cell, and the driver emits an absolute cursor address before
> each one, so a blank cell costs 5 bytes of escape plus the digits of its row
> and column — 8.5 on average at this grid size, and more as the grid grows.
> The dominant term is addressing, not content. Reaching 2 KB needs per-layer
> damage tracking (T-B2,
> [termforge#142](https://github.com/gobha-me/termforge/issues/142), still open)
> and/or run coalescing upstream.
>
> What `test/10frame-bytes` asserts instead, all of it true today: the frame cost
> matches what the wire format independently predicts, every byte is billed to
> `cells` while no plates are drawn, and **a repeated identical frame costs
> exactly zero**. That last one is the property the whole idle budget rests on.
>
> **That test owns the number, and this row does not.** Re-baseline when #142
> lands by changing it there and letting this row keep pointing at it — a figure
> copied into four documents is a figure that will disagree with itself.
>
> One trap for whoever checks this: termforge's own status notes quote ~5.1 KB
> for a 120x40 repaint, which looks like it contradicts 16,344 at 80x24. It does
> not — that measurement is a diffed steady-state frame, not a first paint.
>
> **Correction (2026-08-31).** OBSCURA now requires termforge v0.57.20. The
> earlier block remains the record of v0.7.1, but its stop rule is retired:
> persistent image content now has damage state independent from the cell grid,
> and the fallback driver keeps cursor position across adjacent cell writes.
> `test/10frame-bytes` independently derives the new first-paint wire cost and
> explicitly checks the actual idle frame against the 2 KiB ceiling. The idle
> result is stronger than the budget: an unchanged second frame costs zero.
>
> This does not complete the Bandwidth gate. The dissolve and full-session
> sustained assertions remain owned by #23 and M1 respectively; only the idle
> leg is green today.

## Failure playbook
A hash divergence between compilers or optimisation levels is almost never
"floating point weirdness" — it is uninitialised memory, an unordered container,
or reliance on evaluation order. Bisect with the corpus, not with the game.
