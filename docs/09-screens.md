# Screen specification

Region tables are **normative**; the ASCII figures are cell-accurate renderings of
them. Origin is `(col, row)`, zero-indexed, top-left. Reference grid is exactly
**120 × 40**. Terminals larger than the floor letterbox the view and keep it
centred — the layout never reflows. For the full mixed-state deck plan see Fig. 6
in `OBSCURA-design.html` (open in a browser).

## Modes
Exactly one mode owns the screen. Instruments are modal overlays on `SHIP`, never
permanent chrome.

| Mode | Enter | Owns |
|---|---|---|
| `LOADOUT` | session start | instrument selection; one-way exit — boarding is a commitment |
| `SHIP` | board | hull view, cursor, survey, evidence markers, console bezel |
| `LOG` | `L` | evidence log, scrollback, inline images, filters |
| `AIM` | hold `Space` | commit construction; release confirms, `Esc` aborts free |
| `INSTRUMENT` | `i` | one instrument's readout as its own panel |
| `DEBRIEF` | charge <= 0 / full reconstruction / `q` | score, plate gallery, replay export |

## SHIP — regions
| Region | Origin | Extent | Contents |
|---|---|---|---|
| Hull view | (0,0) | 120 × 33 | Deck plan: 3 bands of 22 × 9 compartment boxes at columns 0, 24, 48, 72, 96 |
| Band 1 (forward) | (—,0) | — × 9 | Bridge · Comms · Medbay · Berth A · Berth B |
| Trunk gutter | (—,9) | — × 2 | Vertical connectors at box-centre columns 10, 34, 58, 106 |
| Band 2 (main) | (—,11) | — × 9 | Fwd airlock · Galley · Hold 1 · Hold 2 · Aft airlock |
| Trunk gutter | (—,20) | — × 2 | Connectors at columns 58, 82 |
| Band 3 (lower aft) | (48,22) | 46 × 9 | Pump bay · Engine room |
| Rule (soot line) | (0,33) | 120 × 1 | Divides hull from bezel |
| Ledger strip | (0,34) | 120 × 2 | Three pending commit slots, 39 cells each, `│` separators at columns 39 and 79 |
| Rule | (0,36) | 120 × 1 | — |
| Console | (0,37) | 120 × 3 | Charge gauge (24 cells), location readout, instrument lamps, key legend |

Compartment boxes are 22 × 9 with a one-cell frame; interior 20 × 7. Horizontal
adjacency draws as `──` in the two-cell gutter at the band's vertical midline;
vertical adjacency as `│` in the trunk gutters. Every edge in the hull graph must
be drawable under these rules — the generator's embedder is constrained by the
renderer, deliberately.

## The three resolution states, 22 × 9 each
```
   UNKNOWN                  SURVEYED                  RESOLVED
   ░▒▓▒░░▓▒░▒▓░░▒▒▓░▒░▓▒    ┌─[ HOLD 1 ]───DMG───┐    ╔═[ HOLD 1 ]═════════╗
   ▒░▓▒░░▒▓░▒▒░▓▒░░▓▒░▒▓    │▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒│    ║▓▒░░▒▓█▓▒░▒▓▓▒░▒▓▒░▒║
   ▓░▒ H?LD ?1 ▒░▓▒░░▒▓░    │▒▒▒◆▒▒▒▒▒▒▒▒▒▒▒◆▒▒▒▒│    ║▒░▒▓██▓▒▒░░▒▓█▓▒░▒▓▒║
   ░▒▓▒░▒▓░░▒▓▒░▒░▓▒▒░░▓    │▒▒┌────────────┐▒▒▒▒│    ║░▒▓█▓▒░▒▓▓▒░▒▓█▓▒░▒▓║
   ▒▓░▒░▒▓▓░▒░░▓▒░▒▓░▒░▒    │▒▒│ ▚▚ 3 EVID ▚│▒▒▒▒│    ║▒▓▓▒░░▒▓█▓▒▒░▒▓▒░░▒▒║
   ░▒▓░▒░▒▓▒░░▒▓▒░▓░▒▒▓░    │▒▒└────────────┘▒▒▒▒│    ║▓▒░▒▓▓▒░▒▓█▓▒░▒▓▒░▓▓║
   ▓▒░░▒▓░▒░▓▒▒░░▓▒░▒▓░▒    │▒▒▒▒▒▒▒◆▒▒▒▒▒▒▒▒▒▒▒▒│    ║▒░░▒▒▓▓▒░░▒▓▒░▒▒▓▓▒░║
   ░▒▓░▒░▓▒░░▒▓▒░▒▓░░▒▒▓    │▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒│    ║▓▓▒▒░░░▒▒▓▓▓▒▒░░▒▒▓▓║
   ▒▓░▒░▓▒░▒▒▓░░▒▓▒░▓░▒▒    └────────────────────┘    ╚════════════════════╝
   glyph band only           glyph + tint bands        hull frame + plate interior
   no image placement        ▒ = damage tint in the     glyph noise cleared; room
   fragments almost legible  cell background, not text  label remains glyph text
```

The resolved frame is `Layer::hull`, not plate pixels. The 240×160 canonical
interior plate is placed with `PlacementFit::Stretch` into the fixed 20×7-cell
interior; kitty enlarges or shrinks it with the terminal's cell geometry. The
room label remains in `Layer::glyph` after the corrupt substrate clears, so one
archetype×damage plate can serve every room of that archetype. Display pixel
geometry changes presentation only and is never simulation or replay input.

## Console bezel — rows 33–39
```
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  1 ▸ QUINT · HOLD 2 · STOW             │  2 ▸ ACHEBE · HOLD 2 · FIGHT FIRE     │  3 ▸ ── empty ──
      ┗ pinned by E01 E02               │      ┗ pinned by E03 E04 E12          │
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  CHARGE ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▒▒▒▒▒▒▒▒▒  74/120 SC       R07 HOLD 1 · DAMAGED · 3 EVIDENCE · 2 EXITS
  LAMP ●   DECRYPTER ●   OSSUARY TAG ●              [e] examine   [SPACE] hold to aim   [R] resolve   [L] log   [?] manual
```
"Pinned by" lists items the player logged whose facts intersect the triple. It is
a memory aid over the player's own log and **never** evaluates correctness.

## LOG — regions
| Region | Origin | Extent | Contents |
|---|---|---|---|
| Header | (0,0) | 120 × 2 | Case name, filter state, item count |
| Scroll | (0,2) | 120 × 36 | `TextBox` scrollback; body wraps at 96 cells; images are Unicode placeholders inline in the flow |
| Console | (0,38) | 120 × 2 | Filter keys, scroll position, exit |

Inline images use Kitty Unicode placeholders — image ID in the cell's foreground
colour, `U+10EEEE` plus row/column diacritics — so salvaged documents scroll as
genuine text, participating in reflow and scrollback. termforge already supports
this, tmux-first. **Build it in M1.** A recovered manifest with the cargo-seal
photograph sitting inside the paragraph and scrolling with it is the screenshot
that sells the project.

## AIM — 72 × 20 at (24,10), on `Layer::overlay`
```
        ┌─[ ASSERT ]──────────────────────────── slot 3 of 3 ──┐
        │                                                      │
        │   ACTOR      ◂  OSRIC BEHN · mate                 ▸   │
        │   WHERE      ◂  R01 COMMS · intact · surveyed     ▸   │
        │   DOING      ◂  BROADCAST                         ▸   │
        │                                                      │
        │   ── supporting ────────────────────────────────────  │
        │   E10  log fragment · decrypter · distress traffic,   │
        │        mate's authentication                          │
        │                                                      │
        │   ── contradicting ────────────────────────────────   │
        │   none                                                │
        │                                                      │
        │   Release SPACE to commit.  Esc aborts, no charge.    │
        │   Batch resolves when all three slots are filled.     │
        │                                                      │
        └──────────────────────────────────────────────────────┘
```
Opened by the **press** of `Space`, closed by its **release**. This is the one
interaction that genuinely requires the Kitty keyboard protocol. The
supporting/contradicting panel must never consult `Veracity` or `Truth` — that
would leak the answer. It is the single most important firewall in the UI layer,
and it gets a test.

## LOADOUT and DEBRIEF — regions
| Mode | Region | Origin | Extent | Contents |
|---|---|---|---|---|
| LOADOUT | Masthead | (0,0) | 120 × 4 | Vessel name, last known position, mass/power gauges |
| LOADOUT | Instrument list | (0,4) | 58 × 30 | `ListWidget`; six instruments, cost and selection state |
| LOADOUT | Detail panel | (60,4) | 60 × 30 | Selected instrument: what it reads, what it cannot |
| LOADOUT | Console | (0,34) | 120 × 6 | Remaining mass/power, board confirmation (irreversible) |
| DEBRIEF | Masthead | (0,0) | 120 × 5 | Outcome: reconstruction %, cause of run end |
| DEBRIEF | Score table | (0,5) | 72 × 24 | Commits locked/failed, charge spent by category, seed |
| DEBRIEF | Plate gallery | (72,5) | 48 × 24 | Thumbnails of every plate earned. The trophy case. |
| DEBRIEF | Console | (0,29) | 120 × 11 | Replay path, verification hash, daily-seed submission, exit |

## Key map
| Key | Event | Mode | Action |
|---|---|---|---|
| `h j k l` / arrows | press, repeat | SHIP | Move the cursor along hull-graph edges; each step costs charge |
| `Enter` | press | SHIP | Survey the compartment under the cursor (8 SC); no-op if surveyed |
| `Tab` | press | SHIP | Cycle evidence markers in the current compartment |
| `e` | press | SHIP | Examine selected evidence (3 SC; 2 SC re-read) |
| `Space` | **press** | SHIP → AIM | Open aim overlay, pre-filled with the cursor's compartment |
| `h l` / `j k` | press, repeat | AIM | Cycle the value / the field of the triple |
| `Space` | **release** | AIM → SHIP | Commit the triple to the pending slot |
| `Esc` | press | AIM | Abort free of charge, even while `Space` is held |
| `1 2 3` | press | SHIP | Select or clear a pending slot |
| `R` | press | SHIP | Resolve the batch; three filled slots only; confirmation modal |
| `L` / `i` / `?` | press | SHIP | Log / instrument panel / manual |
| `q` | press | any | Quit; confirmation modal; writes the replay before exit |

**Gesture rules.** Repeat events in AIM never advance a field more than one step
per event. A release with no matching press (focus loss, renegotiation) **aborts**
rather than commits — fail toward the state that costs the player nothing. If the
terminal stops reporting release events mid-session, pause to a modal and refuse
to continue: a silent downgrade would let a player commit by accident.

> **Correction (2026-07-31).** The Kitty keyboard protocol this spec depends on
> **has landed** in termforge v0.6.3 (issue #60): `KeyAction{Press,Repeat,Release}`,
> `KeyEvent::action`, `KeyboardMode{Legacy,Disambiguate,Enhanced}`, and
> `Capabilities::kitty_keyboard`. Only mid-session *loss* detection (T-E3) is
> still missing.
