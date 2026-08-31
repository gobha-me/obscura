# Tile grammar, ship classes, and layer composition

Confirmed addition to the spec (§6.1, §7.6). This file is the short version; the
canonical text is in `OBSCURA-design.html`.

## The inversion
The hull is **not** a graph that gets drawn. It is a tile layout whose **door
adjacency *is* the graph**. An illegal ship becomes unrepresentable rather than
generated-then-rejected.

```
enum class EdgeKind : uint8_t { hull, bulkhead, door, open };

struct Tile {
  TileId      id;         // manifest-declared integer, NEVER an array index
  Archetype   archetype;
  Damage      damage;
  EdgeKind    n, e, s, w; // the socket signature
  uint8_t     weight;     // integer weight; no floating-point entropy
  VariantMask variants;   // bloodied, scorched, flooded — decals, not tiles
  CellRect    extent;     // 22 x 9 for a compartment box
};
```

## Constraints
- Any tile edge on the grid boundary MUST be `hull`. Exterior walls stop being a
  convention the generator has to remember.
- Adjacent sockets must match: `door`↔`door`, `bulkhead`↔`bulkhead`, `open`↔`open`
  within one compartment. `hull` never faces an interior.
- Every compartment MUST carry at least one `door`. No sealed rooms, ever.
- The hull graph is *derived* from door adjacency, so incident propagation runs
  over real doors rather than an abstract graph.
- Selection uses integer cumulative weights over a **sorted** candidate list from
  `STREAM_HULL`. Backtracking is depth-bounded; exhaustion rejects the seed rather
  than degrading the ship.
- Plausibility is authored into sockets, not coded into the generator: engine-room
  tiles only offer aft-facing hull edges, airlock tiles require a boundary edge.

## Ship classes and room-label precedence
Compartment count is a consequence of ship class, not a die roll.

| Class | Rooms | Labels |
|---|---|---|
| **Required prefix** (every ship) | 6 | bridge · **engine room** · hold · berth · airlock · galley |
| Tender | 6–8 | prefix, then medbay · comms. Pump-bay function folds into the engine room at this size. |
| Freighter — *case 001's class* | 10–14 | prefix + medbay · comms · hold-2 · berth-2 · pump bay · workshop |
| Liner / hauler | 16–24 | the above + second airlock, then repeats of hold and berth to fill |

Every ship has an engine room — *engineering* is a department, not a compartment —
so the engine room sits in the required prefix and the pump bay is the growth-tier
engineering space. **This partly closes DR-05:** the count follows from the class,
so only the class boundaries need tuning against session length, and the
renderer's 15-box cap now has a principled meaning.

## Art is composed, never enumerated
Baking socket configuration into whole tiles multiplies out to thousands of plates
(8 archetypes × 3 damage × 16 edge combos × door positions × variants). The three
lower bands each carry one factor instead, so the asset count stays additive:

| Band | Carries | Count |
|---|---|---|
| `Layer::hull` | structural shell — exterior wall / bulkhead / door pieces per edge kind per orientation | ~12 |
| `Layer::plate` | interior art, archetype × damage | 24 |
| `Layer::overlay` | decals — bloodied, scorched, flooded | ~8 |

A bloodied hallway is a hallway plate **plus** a decal, never a second plate.

## Fixed cells, scaled source pixels

A compartment is always 22 × 9 cells. `Layer::hull` owns its one-cell frame,
leaving a fixed 20 × 7-cell destination for interior art. A plate is a
canonical 240 × 160 source image, placed with `PlacementFit::Stretch`; kitty
scales it to that destination using the terminal's current cell geometry. The
source pixel extent is therefore neither a second layout grid nor a new input
to the run.

The room label remains text in `Layer::glyph` when the compartment resolves.
Only the corrupt glyph substrate clears. Labels are never baked into plates,
so the plate matrix remains archetype × damage rather than multiplying by room
identity. High-DPI interpolation is presentation quality to judge at the M0
real-terminal gate, while the canonical source keeps wire and dirty-edit costs
independent of monitor resolution.

**Packing happens once, at load, into a single resident backing image per
compartment.** Hull, plate and overlay occupy non-overlapping source regions in
that image and are placed from crops at their respective semantic layers. This
means one transmit and one image ID with several cheap placements; it does not
flatten the three bands onto one z-plane. The dissolve edits the plate region
of that one resident image with dirty rects, so T-C1 needs no change and the
bandwidth budget holds. Uploading and animating three independent images would
triple the wire cost of the signature moment and is explicitly not the design.

## Determinism additions
- Tile IDs are **manifest-declared integers covered by the pack hash**, never
  array indices. Shifting IDs when a tile is added would silently make every
  existing replay describe a different ship.
- Weighted selection is integer-only, over a sorted candidate list. The usual
  entropy-driven formulation of this kind of constraint solving is floating-point,
  which violates the no-float rule outright.

## New tickets
- **T-C4** (Epic C, blocks M0) — compose N source images into one resident image
  before placement. Acceptance: one transmit and one image ID, not three.
- **T-D5** (Epic D, blocks M3) — author-declared metadata on atlas entries carried
  through the manifest hash. Stable integer IDs plus opaque per-entry metadata
  (OBSCURA stores socket signatures and weights). The library never interprets it.

> **Correction (2026-07-31).** T-C4 is now filed as
> [gobha-me/termforge#141](https://github.com/gobha-me/termforge/issues/141).
> T-D5 remains unfiled by design (no speculative tickets — it blocks M3); the
> stable-ID half is partly covered by termforge#110.

## What this does and does not solve
**Solved:** layout validity, completely — every room reachable, every boundary
edge a real exterior wall, no orphans, no sealed rooms. That failure class leaves
M3 entirely. It also makes route cost computable: because connectivity is
guaranteed, the cheapest charge cost of a route that surveys every chain-bearing
compartment and reads its items can be bounded, so redaction can reject seeds
whose cheapest full-reconstruction route exceeds the starting charge plus expected
refunds. "Is this seed fair?" becomes arithmetic.

**Not solved:** whether the *deduction* is solvable — that every commit in the key
retains at least one served chain after redaction. That is Appendix A.6 mechanised
and remains the hardest work in M3. The tile grammar means the solver no longer
reasons about geometry, only about facts and instrument gating.

Net: M3 drops from two hard problems to one hard problem plus arithmetic. Still
last in the order, still upside rather than foundation.
