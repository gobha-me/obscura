# M0 — Render spike

**Where:** inside **termforge**, as an example plus tests. Do **not** create the
OBSCURA repository yet.
**Gate:** *Does the dissolve feel good?* This is the whole thesis. If it does not
land, the concept is wrong and stopping here costs a week.
**Estimate:** ~1 week of library work. Nothing here is game logic.

## Why this order
Three of the five compositing bands, the dissolve mechanism, and the byte meter
do not exist in termforge yet. Building them inside the library gets you its test
harness, its both-compiler CI, and a spike you can throw away without losing a
game repo.

## Tickets to file first
T-H4 (byte meter — do this first, it is an afternoon and everything else is
measured by it), T-H5 (cell pixel geometry), T-B1 (layer API), T-B2 (per-layer
damage), T-C1 (resident-image frame edits), T-C2 (per-frame gaps), T-D1 (image
lifecycle). Comment the OBSCURA use case on the ones that exist.

## Tasks
- [ ] **T-H4 byte meter.** Bytes-written-per-frame counter on the driver, readable
      by the app. Everything below is measured against it.
- [ ] **T-H5 cell geometry.** Verify whether existing pixel-region plumbing already
      exposes cell width/height in px and its change on resize/font change. If it
      does, close the ticket with a doc note instead of writing code.
- [ ] **T-B1 layer API.** Named layers: `hull` (images below cell backgrounds),
      `tint` (cell backgrounds), `plate` (images z<0), `glyph` (text),
      `overlay` (images z>=0). The below-background sentinel
      (`-1073741825`, icat's `--1`) appears exactly once, in the library.
- [ ] **T-B2 per-layer damage.** Dirtying the tint band must not re-emit plates.
      Assert with the meter in the test.
- [ ] **Five-band composite example.** One example that puts distinguishable
      content in all five bands simultaneously and proves the ordering. Offline
      byte-level test plus a screenshot from a real emulator (ask the human).
- [ ] **One static art plate.** 240x160 px, 4 colours + transparent, 1-bit ordered
      dither, PNG-compressed. Measure the on-wire cost; target <= 8 KB.
- [ ] **T-C1 / T-C2 frame edits.** Edit rectangular blocks of a resident image
      frame; per-frame gaps in ms; gapless frames as composition bases. A
      dirty-rect edit must emit an animation command, **not** a retransmit.
- [ ] **The dissolve.** 400 ms, ~13 dirty-rect edits, <= 40 KB total:
      0–160 ms ordered-dither reveal (8 frames); 160–300 ms glyph attrition in
      reverse structural order (4 frames); 300–400 ms tint drain. Skippable by
      keypress, and skipping jumps to the final composition.
- [ ] **T-D1 image lifecycle tests — write these before anything else that uses
      images.** Coverage: clear-screen mid-dissolve; alt-screen leave/return with
      plates resident; SIGWINCH mid-dissolve; quota exhaustion forcing an eviction
      while a dissolve is in flight; process death mid-transmission.
- [ ] **Bandwidth assertions.** Idle frame <= 2 KB; single dissolve <= 40 KB;
      sustained <= 250 KB/min. As tests, not as notes.

## Exit criteria
1. All five bands demonstrably ordered, verified offline **and** on a real emulator.
2. Dissolve runs at 400 ms under 40 KB with no retransmit.
3. Lifecycle suite green; no ghost images under any of the five scenarios.
4. Meter live and asserted.
5. Zero lines of game logic written.

## Gate ritual
Watch the dissolve fifty times. Then hand it to someone else and watch their face.
If nobody leans in, write down why and stop — the answer is worth a week either way.

---
> **Correction (2026-07-31).** Ticket numbers are now: T-H4 = termforge#139,
> T-H5 = #143, T-B1 = #114 (pre-existing, needs a fifth plane), T-B2 = #142,
> T-C1 = #140, T-C2 = #116 (pre-existing), T-C4 = #141, T-D1 = #113
> (pre-existing). Tracked here as issues #16–#26; the M0 gate is issue #26.
>
> **Correction (2026-08-04).** **T-H4 is done** — #139 landed in termforge
> v0.6.8, and this repo now pins v0.7.1. Two consequences for the task list
> above.
>
> The **"Bandwidth assertions"** task is blocked on T-B2 (#142), not on the
> meter. A full repaint measures 16,344 bytes at 80x24 — the driver emits an
> absolute cursor address before every cell the renderer hands it — so the 2 KB
> line cannot be asserted yet. See `docs/08-determinism.md` for what to assert
> instead; the number itself is owned by `test/10frame-bytes`.
>
> The **"one static art plate"** task got *easier*. termforge#163 (v0.6.9) added
> a pre-encoded transmit path — `EncodedImage` with `ImageFormat::Png` — so a
> plate ships its own compressed bytes rather than base64'd raw RGBA. Upstream
> measured a 240x160 4-colour ordered-dithered PNG at **3,952 B asset → 5,272 B
> base64**, against **204,800 B** for the same image as RGBA. Both figures
> exclude the APC framing (a few dozen bytes per 4096-byte chunk), which is
> additive rather than part of the ~1.34x base64 factor.
>
> Two cautions before treating that as this task's answer. The **8 KB target
> above is already a wire quantity** ("measure the on-wire cost"), so it needs no
> restating — an 8 KB wire budget allows roughly a 6 KB asset. And the measured
> plate is upstream's *synthetic* test image, chosen deliberately to compress
> badly, and it is opaque where this spec calls for 4 colours **+ transparent**.
> It demonstrates the mechanism; it does not predict what authored art will cost.
> Measure the real plate.
>
> **Correction (2026-08-04, later).** The real plate is measured, and the
> **"one static art plate"** task is done — issue #21. `assets/plates/hold-d0.png`
> is baked by `tools/venice-bake/`, compiled in as a `constexpr` array, and its
> on-wire cost is asserted against the meter by **`test/11art-plate`, which owns
> that figure**. It is not restated here, for the reason the block above gives
> about 16,344.
>
> What is worth recording here is the *shape* of the answer rather than the
> number. Authored 4-colour art compresses far better than upstream's synthetic
> probe did — flat fills and periodic dither are what deflate is good at — so
> the plate lands well inside the budget with room to spare. Do not spend that
> room in advance: #23's dissolve budget is a separate 40 KB, and the headroom
> here says nothing about a plate at damage 2 with heavy scorch dither.
>
> Two corrections to the block above, both verified against the pinned v0.7.1
> source rather than against upstream's status notes:
>
> - **`image_transmit` includes the APC framing.** `KittyDriver::draw_image`
>   tallies the whole byte range it appends around `transmit()`, so the meter
>   reports base64 *plus* framing. Upstream's 5,272 figure is base64 alone; the
>   metered number for that payload is 5,324. Both are right about different
>   quantities, and issue #21's comment thread has them the other way round.
> - **The 8 KB cap is on-wire, and the wire carries the placement too.** The
>   payload therefore has to fit in rather less than 8,192 once the cursor
>   address and `a=p` are paid — which is why the configure-time gate in
>   `src/lib/CMakeLists.txt` is tighter than transmit-only arithmetic suggests.
>
> **Correction (2026-08-31, plate layout).** Issues #57/#58 reconcile the two
> extents without making terminal pixels part of the layout. A compartment
> remains 22x9 cells; its hull-owned one-cell frame leaves a 20x7 interior. The
> canonical 240x160 interior plate uses `PlacementFit::Stretch` into that rect,
> and the room label remains glyph text after the noise clears. The plate asset
> therefore stays archetype x damage and needs no re-bake.
>
> **Correction (2026-08-31).** This repo now requires termforge v0.57.20. The
> upstream facilities behind T-B1, T-B2, T-C1, T-C2, T-C4 and T-D1 are present:
> named image regimes, persistent pixel damage separate from cell damage,
> `edit_pinned()`, exact per-frame gaps (including zero), compose-then-submit as
> one persistent surface, and explicit lifecycle invalidation. That retires
> OBSCURA #18, #19, #22 and #24 as dependency blockers.
>
> The byte oracle moved with the dependency. Cursor-aware sequential writes
> make an 80x24 first paint nearly fit the idle ceiling; an unchanged frame is
> still exactly zero. Correlated image replies add four bytes to a multi-chunk
> continuation, and the scaled below-text placement adds `c=20,r=7,z=-1`, so
> the configure-time maximum plate payload is now 6,069 bytes.
> `test/10frame-bytes` and `test/11art-plate` remain the owners of the measured
> figures. The real committed plate is still one chunk and its transmit cost is
> unchanged; the complete placement cost now includes the scale and layer keys.
