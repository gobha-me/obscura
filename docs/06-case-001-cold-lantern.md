# Case 001 — *Cold Lantern*

The M1 authored case, complete. Transcribe this into
`cases/case001_cold_lantern.cpp` as C++ constexpr data (DR-13) — do not
re-invent it. Twelve compartments, seven crew, nine timesteps, twenty-two derived
evidence items redacted to sixteen.

**Published loadout:** spectral lamp + decrypter + ossuary tag (mass 4, power 3 of
6/4). This deliberately leaves `damage_pattern` evidence visible but unreadable.

## Hull — 12 compartments, 15 edges
| ID | Name | Archetype | Grid origin | Final damage | Adjacent |
|---|---|---|---|---|---|
| R00 | Bridge | bridge | (0,0) | intact | R01, R05 |
| R01 | Comms | comms | (24,0) | intact | R00, R04, R06 |
| R04 | Medbay | medbay | (48,0) | intact | R01, R02, R07 |
| R02 | Berth A | berth | (72,0) | intact | R04, R03 |
| R03 | Berth B | berth | (96,0) | intact | R02, R09 |
| R05 | Fwd airlock | airlock | (0,11) | intact | R00, R06 |
| R06 | Galley | galley | (24,11) | intact | R05, R01, R07 |
| R07 | Hold 1 | hold | (48,11) | **damaged** | R06, R04, R08, R10 |
| R08 | Hold 2 | hold | (72,11) | **breached** | R07, R09, R11 |
| R09 | Aft airlock | airlock | (96,11) | intact | R08, R03 |
| R10 | Pump bay | engineering | (48,22) | **damaged** | R07, R11 |
| R11 | Engine room | engineering | (72,22) | intact | R10, R08 |

Every edge is drawable under the deck-plan rules: horizontal within a band,
vertical between bands, no diagonals. Engineering is aft and low, the bridge is
forward, both airlocks sit on the hull boundary, no berth opens into a hold.

## Root cause
`contraband reaction`, origin **R08 Hold 2**. An undeclared oxidiser-class seal,
stowed under the supercargo's authority at T0, reacted at T2. Fire spread forward
into Hold 1 at T3. Hold 2 breached at T4.

## Crew — 7 actors, T0..T8
| ID | Name | Role | Timeline (T, room, action) |
|---|---|---|---|
| A0 | Ilse Vantner | Master | T0–T4 R00; **T5 R00 vent**; T6–T7 → R05; T8 R05 abandon |
| A1 | Osric Behn | Mate | T0–T3 R00; T4 → R01, **T4 R01 broadcast**; T6–T7 → R05; T8 R05 abandon |
| A2 | Duna Karr | Engineer | T0–T2 R10; **T3 R10 isolate**; T6 → R11; T7 → R08 (breached, suited); T8 R09 abandon |
| A3 | Fen Achebe | Oiler | T0–T1 R11; T2 → R08; **T3 R08 fight_fire**; **T4 R08 die** |
| A4 | Tobin Reyes | Medic | T0–T2 R04; T3 → R07; T5 → R04; **T6 R04 treat**; T7–T8 → R05 abandon |
| A5 | Marisol Quint | Supercargo | **T0 R08 stow**; T1 → R07; T3 → R06; T6–T7 → R05; **T8 R05 abandon** |
| A6 | Yeo Halim | Deckhand | T0 R06; T1 → R07; **T3 R07 seal**; T5 → R04; T6 R04 (treated); T8 R05 abandon |

Karr's T7 exit crosses the breached Hold 2 in a suit. That transit is the case's
best trap: it leaves an honest trace in the compartment where the corpse lies.

## Solution key — 9 commits, 3 batches
| Batch | # | Actor | Compartment | Action | Reading |
|---|---|---|---|---|---|
| A | 1 | Quint | R08 Hold 2 | `stow` | The contraband went in under her chop. |
| A | 2 | Achebe | R08 Hold 2 | `fight_fire` | Somebody stood and fought it. |
| A | 3 | Achebe | R08 Hold 2 | `die` | He did not leave. |
| B | 4 | Halim | R07 Hold 1 | `seal` | Dogged from the forward side — with Achebe behind it. |
| B | 5 | Vantner | R00 Bridge | `vent` | The master vented Hold 1 to kill the fire. |
| B | 6 | Behn | R01 Comms | `broadcast` | The mate sent the distress traffic. |
| C | 7 | Karr | R10 Pump bay | `isolate` | Why she was not in the hold. |
| C | 8 | Reyes | R04 Medbay | `treat` | One patient, smoke inhalation, the deckhand. |
| C | 9 | Quint | R05 Fwd airlock | `abandon` | She left with the boat, and the second ledger. |

Batch A alone is the incident. A+B is the partial-credit ending (6 of 9, DR-03).
All nine is full reconstruction.

## Evidence served — 16 of 22
| ID | Room | Kind | Requires | Veracity | Asserts / reads as |
|---|---|---|---|---|---|
| E01 | R08 | cargo_seal | — | TRUE | (—, T0, R08, stow) undeclared oxidiser-class seal stowed here |
| E02 | R07 | manifest | — | TRUE | (Quint, ANY, ANY, stow) stowage authority, her chop in the wax; seal number not in the declared list |
| E03 | R08 | corpse | ossuary tag | TRUE | (Achebe, ANY, R08, die) — *without the tag: unidentified* |
| E04 | R08 | physical_trace | spectral lamp | TRUE | (—, ANY, R08, fight_fire) discharge from hand height, abandoned mid-sweep |
| E05 | R11 | personal_effect | — | TRUE | (Achebe, ANY, R11, ANY) oiler's tally board, shift signed on |
| E06 | R07 | damage_pattern | **thermal tap** | TRUE | (—, T3, R07, ANY) **unreadable in case 1** — the teaching beat |
| E07 | R07 | physical_trace | spectral lamp | TRUE | (Halim, ANY, R07, seal) palm salts on the dog-lever, one set of hands |
| E08 | R06 | personal_effect | — | **STALE** | (Halim, ANY, R06, ANY) half-finished provision list — true at T0 only |
| E09 | R00 | log_fragment | decrypter | TRUE | (Vantner, T5, R00, vent) "MASTER: VENTING HOLD 1 ON MY ORDER" |
| E10 | R01 | log_fragment | decrypter | TRUE | (Behn, T4, R01, broadcast) distress traffic, mate's authentication |
| E12 | R10 | physical_trace | spectral lamp | TRUE | (Karr, ANY, R10, isolate) valve wheel wiped, chalk mark on the isolation tag |
| E13 | R11 | personal_effect | — | **STALE** | (Karr, ANY, R11, ANY) engineer's tea can, still lashed |
| E14 | R04 | log_fragment | decrypter | TRUE | (Reyes, T6, R04, treat) "smoke inhalation, one patient, deckhand" |
| E16 | R05 | manifest | — | TRUE | (—, T8, R05, abandon) boat log: five out |
| E17 | R05 | personal_effect | — | TRUE | (Quint, T8, R05, abandon) supercargo's seal press, dropped in the cycle |
| E18 | R08 | personal_effect | — | TRUE | (Karr, ANY, R08, ANY) left-cuff engineer's glove, scorched palm, eleven feet from the corpse — **the trap** |

No `MISLEADING` items in case 1 (DR-06); two `STALE`. The most valuable evidence
is honest but easy to misread: E18 is TRUE and is the item most likely to cost a
player a batch. Design for that, not for lies.

## Redaction pass — 6 cut
| Cut | Was | Why |
|---|---|---|
| E11 | Master's order book, R00 | Redundant with E09, which is stronger and instrument-gated |
| E15 | Soot transfer on the medbay cot rail | Redundant with E14 — "patient: deckhand" already identifies Halim |
| E19 | Aft boat cycle log, R09 | Pins a fact no commit needs; its absence leaves Karr's exit slightly mysterious, which is a feature |
| E20 | Second contraband ledger, R03 | Over-determines Quint; made commit 1 free |
| E21 | Reaction-origin burn pattern, R08 | Needs the thermal tap, not in the published loadout. One unreadable item teaches the lesson; two is tax |
| E22 | Behn's trace in comms | Redundant with E10 |

Rule applied: cut redundancy and cut frustration, never cut the last chain to a
commit.

## Solvability proof
The table M3's solver must produce mechanically for a generated seed.

| # | Chain | Argument |
|---|---|---|
| 1 | E01 ∩ E02 | E01 places a stow of an undeclared seal in R08 at T0; E02 makes Quint the only actor with stowage authority. Unique intersection. |
| 2 | E04 ∩ E03 ∩ (E05, E12) | E04 says someone fought the fire in R08. Two actors are placed in R08 by evidence: Achebe (E03) and Karr (E18). E12 places Karr in R10 doing `isolate`; E05 places Achebe's shift in the adjacent R11. The firefighter is Achebe. |
| 3 | E03 + ossuary tag | Corpse in R08; the tag supplies identity and role. Unreachable without the tag — which is why the tag is in the published loadout. |
| 4 | E07 | Palm salts on the dog-lever, one set, Halim. E08 (stale) invites the wrong answer; E07 outranks it because it is an instrument reading of the lever itself. |
| 5 | E09 | Direct, decrypter-gated. |
| 6 | E10 | Direct, decrypter-gated. |
| 7 | E12 | Direct. Also the load-bearing step of chain 2 — one item, two jobs, which is what good redaction leaves behind. |
| 8 | E14 | "One patient, deckhand" — Halim is the only deckhand in the roster the player already has. |
| 9 | E17 ∩ E16 | E17 places Quint's seal press in the forward airlock cycle; E16's "five out" confirms an abandon, not a body. |

## Cost check (tune here first)
Served evidence sits in nine compartments: R08 (4), R07 (3), R11 (2), R05 (2),
R10, R00, R01, R06, R04 (1 each).

- Survey all nine: 9 × 8 = **72 SC**
- Movement: about **16 SC**
- Read all sixteen items: 16 × 3 = **48 SC**
- Total **136 SC** against a starting **120 SC**

One correct batch (+15) makes it reachable; two make it comfortable. The case
therefore *requires* committing before the player feels ready — the intended
pressure. If M1 playtests feel cruel, lower survey cost before raising the
starting charge; if they feel slack, cut the refund before adding costs.
