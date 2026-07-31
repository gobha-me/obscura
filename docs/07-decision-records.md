# Decision records

Four are decided by the spec. The rest are open, with tradeoffs and a decide-by
milestone. Record the outcome here when each closes.

| DR | Question | Tradeoff | Leaning / decide by |
|---|---|---|---|
| 01 | Does batch size scale with difficulty, or stay at three? | Scaling gives a free difficulty dial. Fixed keeps the gesture's feel identical across cases and keeps the ledger strip at 3 × 39 cells. | Fixed. Decide by M2 (case 5). |
| 02 | One budget pool, or separate survey / failure pools? | Two pools tune punishment for bad deduction independently of exploration. One pool is legible on a single gauge and makes every decision commensurable. | Start single. Revisit at M1 tuning only if playtests show players refusing to survey. |
| 03 | How much of the incident should be reconstructable? | Full-only is brutal and ends most runs in failure. Partial credit gives a landing for a good-but-incomplete run and a mastery target above it. | Partial credit at 6 of 9 commits; full reconstruction as a rare mastery outcome. Decide by M1. |
| 04 | Does the narrator help atmosphere or undercut the austerity? | A voice is atmosphere and accessibility. It also breaks the conceit that you are reading instruments, not being told a story. | Build it, ship it off by default, test in M2, cut without ceremony. |
| 05 | Is 12–24 compartments right? | More compartments means more survey cost and longer sessions; fewer collapses the deduction space. The screen also constrains it — the deck plan holds 15 boxes. | 12 for case 1. Tune against the 30–45 min target in M1; renderer caps at 15 until the embedder gets smarter. |
| 06 | Should `MISLEADING` evidence exist in case 1? | Lies immediately teach distrust before the player trusts anything. Withholding makes case 2 a step change. | **Decided for case 1:** none; two `STALE` instead. Introduce `MISLEADING` in case 2. |
| 07 | Is audio tier 1 or tier 2? | Atmosphere is a large part of dread. Information in audio breaks ssh play, accessibility and the determinism story. | **Decided:** interface tier 1 (M1), RtAudio tier 2 (M2), no audio-only information ever. |
| 08 | Where does RtAudio live? | Inside termforge it is convenient and breaks the stdlib-only guarantee. Outside costs one more CMake project. | **Decided:** separate optional target; interface only in termforge. |
| 09 | Does the replay record ticks or only input order? | Ticks reproduce animation exactly but make wall-clock jitter part of the artifact. Input-order-only makes verification machine-independent. | **Decided:** input order only; the tick is cosmetic. |
| 10 | Public GitHub now, or private Gitea first? | Public early gets the audience and free scrutiny of the determinism claims. Private early lets M0 fail quietly, which matters when its gate is "does this feel good". | Private Gitea through the M0 gate; mirror public at M1. Keep ticket bodies platform-neutral and mirrored in `docs/tickets/` so IDs are never the source of truth. |
| 11 | Epics with tickets, or tickets alone? | Epics survive re-planning; flat tickets are easier to pick up cold. | Epics A–H with tickets inside them. Refine bodies in the first working session with the implementer. |
| 12 | What is the minimum terminal, and what happens below it? | A smaller floor widens the audience and complicates every layout. A fixed reference grid makes the deck plan authorable. | **Decided:** 120 × 40 floor, fixed reference grid, letterbox above, refuse below, pause-to-modal on shrink. |
| 13 | Are authored cases data files or C++ data? | A data format needs a parser, a schema, error handling and its own determinism story. C++ data is compile-time checked with none of that, but no external author can write a case. | **Decided for now:** C++ constexpr data under `cases/`. Revisit when someone other than you authors a case. |
| 14 | Does a commit assert *when*, as well as who/where/what? | Time turns a 3-tuple into a 4-tuple and roughly squares the assertion space — much harder, much more interesting. It also adds a fourth field to the aim overlay. | Not in M1. Candidate difficulty axis for cases 3–5 and a natural "advanced" mode. Decide by M2. |

---

> **Correction (2026-07-31) — DR-10 is now decided.**
> The repo is **`gobha-me/obscura` on GitHub, private**, created at the design
> phase rather than after the M0 gate. The reasoning behind DR-10 is preserved:
> private through the M0 gate so an M0 failure can be quiet, then
> `gh repo edit --visibility public` at M1. Ticket bodies remain host-neutral
> and mirrored, so issue IDs are still not the source of truth.
>
> **DR-11 is applied.** Epics A–H exist as tracking issues (upstream/library
> work), alongside game-side epics W, R, N, S, V, K, Y.
>
> **DR-05 is partly closed by the tile grammar** — compartment count now follows
> from ship class rather than a die roll, so only the class boundaries need
> tuning. See `10-tile-grammar.md`.

## Open decisions, as issues
| DR | Issue | Decide by |
|---|---|---|
| 01 | [#44](https://github.com/gobha-me/obscura/issues/44) | M2 |
| 02 | [#45](https://github.com/gobha-me/obscura/issues/45) | M1 |
| 03 | [#46](https://github.com/gobha-me/obscura/issues/46) | M1 |
| 04 | [#47](https://github.com/gobha-me/obscura/issues/47) | M2 |
| 05 | [#48](https://github.com/gobha-me/obscura/issues/48) | M1 / M3 |
| 14 | [#49](https://github.com/gobha-me/obscura/issues/49) | M2 |
