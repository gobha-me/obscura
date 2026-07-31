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

## Failure playbook
A hash divergence between compilers or optimisation levels is almost never
"floating point weirdness" — it is uninitialised memory, an unordered container,
or reliance on evaluation order. Bisect with the corpus, not with the game.
